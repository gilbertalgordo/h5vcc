// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser.
 */
var BrowserBridge = (function() {
  'use strict';

  /**
   * Delay in milliseconds between updates of certain browser information.
   */
  var POLL_INTERVAL_MS = 5000;

  /**
   * @constructor
   */
  function BrowserBridge() {
    assertFirstConstructorCall(BrowserBridge);

    // List of observers for various bits of browser state.
    this.connectionTestsObservers_ = [];
    this.hstsObservers_ = [];
    this.constantsObservers_ = [];
    this.crosONCFileParseObservers_ = [];
    this.storeDebugLogsObservers_ = [];
    this.setNetworkDebugModeObservers_ = [];
    // Unprocessed data received before the constants.  This serves to protect
    // against passing along data before having information on how to interpret
    // it.
    this.earlyReceivedData_ = [];

    this.pollableDataHelpers_ = {};
    this.pollableDataHelpers_.proxySettings =
        new PollableDataHelper('onProxySettingsChanged',
                               this.sendGetProxySettings.bind(this));
    this.pollableDataHelpers_.badProxies =
        new PollableDataHelper('onBadProxiesChanged',
                               this.sendGetBadProxies.bind(this));
    this.pollableDataHelpers_.httpCacheInfo =
        new PollableDataHelper('onHttpCacheInfoChanged',
                               this.sendGetHttpCacheInfo.bind(this));
    this.pollableDataHelpers_.hostResolverInfo =
        new PollableDataHelper('onHostResolverInfoChanged',
                               this.sendGetHostResolverInfo.bind(this));
    this.pollableDataHelpers_.socketPoolInfo =
        new PollableDataHelper('onSocketPoolInfoChanged',
                               this.sendGetSocketPoolInfo.bind(this));
    this.pollableDataHelpers_.sessionNetworkStats =
      new PollableDataHelper('onSessionNetworkStatsChanged',
                             this.sendGetSessionNetworkStats.bind(this));
    this.pollableDataHelpers_.historicNetworkStats =
      new PollableDataHelper('onHistoricNetworkStatsChanged',
                             this.sendGetHistoricNetworkStats.bind(this));
    this.pollableDataHelpers_.spdySessionInfo =
        new PollableDataHelper('onSpdySessionInfoChanged',
                               this.sendGetSpdySessionInfo.bind(this));
    this.pollableDataHelpers_.spdyStatus =
        new PollableDataHelper('onSpdyStatusChanged',
                               this.sendGetSpdyStatus.bind(this));
    this.pollableDataHelpers_.spdyAlternateProtocolMappings =
        new PollableDataHelper('onSpdyAlternateProtocolMappingsChanged',
                               this.sendGetSpdyAlternateProtocolMappings.bind(
                                   this));
    if (cr.isWindows) {
      this.pollableDataHelpers_.serviceProviders =
          new PollableDataHelper('onServiceProvidersChanged',
                                 this.sendGetServiceProviders.bind(this));
    }
    this.pollableDataHelpers_.prerenderInfo =
        new PollableDataHelper('onPrerenderInfoChanged',
                               this.sendGetPrerenderInfo.bind(this));
    this.pollableDataHelpers_.httpPipeliningStatus =
        new PollableDataHelper('onHttpPipeliningStatusChanged',
                               this.sendGetHttpPipeliningStatus.bind(this));

    // Setting this to true will cause messages from the browser to be ignored,
    // and no messages will be sent to the browser, either.  Intended for use
    // when viewing log files.
    this.disabled_ = false;

    // Interval id returned by window.setInterval for polling timer.
    this.pollIntervalId_ = null;
  }

  cr.addSingletonGetter(BrowserBridge);

  BrowserBridge.prototype = {

    //--------------------------------------------------------------------------
    // Messages sent to the browser
    //--------------------------------------------------------------------------

    /**
     * Wraps |chrome.send|.  Doesn't send anything when disabled.
     */
    send: function(value1, value2) {
      if (!this.disabled_) {
        if (arguments.length == 1) {
          chrome.send(value1);
        } else if (arguments.length == 2) {
          chrome.send(value1, value2);
        } else {
          throw 'Unsupported number of arguments.';
        }
      }
    },

    sendReady: function() {
      this.send('notifyReady');
      this.setPollInterval(POLL_INTERVAL_MS);
    },

    /**
     * Some of the data we are interested is not currently exposed as a
     * stream.  This starts polling those with active observers (visible
     * views) every |intervalMs|.  Subsequent calls override previous calls
     * to this function.  If |intervalMs| is 0, stops polling.
     */
    setPollInterval: function(intervalMs) {
      if (this.pollIntervalId_ !== null) {
        window.clearInterval(this.pollIntervalId_);
        this.pollIntervalId_ = null;
      }

      if (intervalMs > 0) {
        this.pollIntervalId_ =
            window.setInterval(this.checkForUpdatedInfo.bind(this, false),
                               intervalMs);
      }
    },

    sendGetProxySettings: function() {
      // The browser will call receivedProxySettings on completion.
      this.send('getProxySettings');
    },

    sendReloadProxySettings: function() {
      this.send('reloadProxySettings');
    },

    sendGetBadProxies: function() {
      // The browser will call receivedBadProxies on completion.
      this.send('getBadProxies');
    },

    sendGetHostResolverInfo: function() {
      // The browser will call receivedHostResolverInfo on completion.
      this.send('getHostResolverInfo');
    },

    sendRunIPv6Probe: function() {
      this.send('onRunIPv6Probe');
    },

    sendClearBadProxies: function() {
      this.send('clearBadProxies');
    },

    sendClearHostResolverCache: function() {
      this.send('clearHostResolverCache');
    },

    sendClearBrowserCache: function() {
      this.send('clearBrowserCache');
    },

    sendClearAllCache: function() {
      this.sendClearHostResolverCache();
      this.sendClearBrowserCache();
    },

    sendStartConnectionTests: function(url) {
      this.send('startConnectionTests', [url]);
    },

    sendHSTSQuery: function(domain) {
      this.send('hstsQuery', [domain]);
    },

    sendHSTSAdd: function(domain, include_subdomains, pins) {
      this.send('hstsAdd', [domain, include_subdomains, pins]);
    },

    sendHSTSDelete: function(domain) {
      this.send('hstsDelete', [domain]);
    },

    sendGetHttpCacheInfo: function() {
      this.send('getHttpCacheInfo');
    },

    sendGetSocketPoolInfo: function() {
      this.send('getSocketPoolInfo');
    },

    sendGetSessionNetworkStats: function() {
      this.send('getSessionNetworkStats');
    },

    sendGetHistoricNetworkStats: function() {
      this.send('getHistoricNetworkStats');
    },

    sendCloseIdleSockets: function() {
      this.send('closeIdleSockets');
    },

    sendFlushSocketPools: function() {
      this.send('flushSocketPools');
    },

    sendGetSpdySessionInfo: function() {
      this.send('getSpdySessionInfo');
    },

    sendGetSpdyStatus: function() {
      this.send('getSpdyStatus');
    },

    sendGetSpdyAlternateProtocolMappings: function() {
      this.send('getSpdyAlternateProtocolMappings');
    },

    sendGetServiceProviders: function() {
      this.send('getServiceProviders');
    },

    sendGetPrerenderInfo: function() {
      this.send('getPrerenderInfo');
    },

    enableIPv6: function() {
      this.send('enableIPv6');
    },

    setLogLevel: function(logLevel) {
      this.send('setLogLevel', ['' + logLevel]);
    },

    refreshSystemLogs: function() {
      this.send('refreshSystemLogs');
    },

    getSystemLog: function(log_key, cellId) {
      this.send('getSystemLog', [log_key, cellId]);
    },

    importONCFile: function(fileContent, passcode) {
      this.send('importONCFile', [fileContent, passcode]);
    },

    storeDebugLogs: function() {
      this.send('storeDebugLogs');
    },

    setNetworkDebugMode: function(subsystem) {
      this.send('setNetworkDebugMode', [subsystem]);
    },

    sendGetHttpPipeliningStatus: function() {
      this.send('getHttpPipeliningStatus');
    },

    //--------------------------------------------------------------------------
    // Messages received from the browser.
    //--------------------------------------------------------------------------

    receive: function(command, params) {
      // Does nothing if disabled.
      if (this.disabled_)
        return;

      // If no constants have been received, and params does not contain the
      // constants, delay handling the data.
      if (Constants == null && command != 'receivedConstants') {
        this.earlyReceivedData_.push({ command: command, params: params });
        return;
      }

      this[command](params);

      // Handle any data that was received early in the order it was received,
      // once the constants have been processed.
      if (this.earlyReceivedData_ != null) {
        for (var i = 0; i < this.earlyReceivedData_.length; i++) {
          var command = this.earlyReceivedData_[i];
          this[command.command](command.params);
        }
        this.earlyReceivedData_ = null;
      }
    },

    receivedConstants: function(constants) {
      for (var i = 0; i < this.constantsObservers_.length; i++)
        this.constantsObservers_[i].onReceivedConstants(constants);
    },

    receivedLogEntries: function(logEntries) {
      EventsTracker.getInstance().addLogEntries(logEntries);
    },

    receivedProxySettings: function(proxySettings) {
      this.pollableDataHelpers_.proxySettings.update(proxySettings);
    },

    receivedBadProxies: function(badProxies) {
      this.pollableDataHelpers_.badProxies.update(badProxies);
    },

    receivedHostResolverInfo: function(hostResolverInfo) {
      this.pollableDataHelpers_.hostResolverInfo.update(hostResolverInfo);
    },

    receivedSocketPoolInfo: function(socketPoolInfo) {
      this.pollableDataHelpers_.socketPoolInfo.update(socketPoolInfo);
    },

    receivedSessionNetworkStats: function(sessionNetworkStats) {
      this.pollableDataHelpers_.sessionNetworkStats.update(sessionNetworkStats);
    },

    receivedHistoricNetworkStats: function(historicNetworkStats) {
      this.pollableDataHelpers_.historicNetworkStats.update(
          historicNetworkStats);
    },

    receivedSpdySessionInfo: function(spdySessionInfo) {
      this.pollableDataHelpers_.spdySessionInfo.update(spdySessionInfo);
    },

    receivedSpdyStatus: function(spdyStatus) {
      this.pollableDataHelpers_.spdyStatus.update(spdyStatus);
    },

    receivedSpdyAlternateProtocolMappings:
        function(spdyAlternateProtocolMappings) {
      this.pollableDataHelpers_.spdyAlternateProtocolMappings.update(
          spdyAlternateProtocolMappings);
    },

    receivedServiceProviders: function(serviceProviders) {
      this.pollableDataHelpers_.serviceProviders.update(serviceProviders);
    },

    receivedStartConnectionTestSuite: function() {
      for (var i = 0; i < this.connectionTestsObservers_.length; i++)
        this.connectionTestsObservers_[i].onStartedConnectionTestSuite();
    },

    receivedStartConnectionTestExperiment: function(experiment) {
      for (var i = 0; i < this.connectionTestsObservers_.length; i++) {
        this.connectionTestsObservers_[i].onStartedConnectionTestExperiment(
            experiment);
      }
    },

    receivedCompletedConnectionTestExperiment: function(info) {
      for (var i = 0; i < this.connectionTestsObservers_.length; i++) {
        this.connectionTestsObservers_[i].onCompletedConnectionTestExperiment(
            info.experiment, info.result);
      }
    },

    receivedCompletedConnectionTestSuite: function() {
      for (var i = 0; i < this.connectionTestsObservers_.length; i++)
        this.connectionTestsObservers_[i].onCompletedConnectionTestSuite();
    },

    receivedHSTSResult: function(info) {
      for (var i = 0; i < this.hstsObservers_.length; i++)
        this.hstsObservers_[i].onHSTSQueryResult(info);
    },

    receivedONCFileParse: function(error) {
      for (var i = 0; i < this.crosONCFileParseObservers_.length; i++)
        this.crosONCFileParseObservers_[i].onONCFileParse(error);
    },

    receivedStoreDebugLogs: function(status) {
      for (var i = 0; i < this.storeDebugLogsObservers_.length; i++)
        this.storeDebugLogsObservers_[i].onStoreDebugLogs(status);
    },

    receivedSetNetworkDebugMode: function(status) {
      for (var i = 0; i < this.setNetworkDebugModeObservers_.length; i++)
        this.setNetworkDebugModeObservers_[i].onSetNetworkDebugMode(status);
    },

    receivedHttpCacheInfo: function(info) {
      this.pollableDataHelpers_.httpCacheInfo.update(info);
    },

    receivedPrerenderInfo: function(prerenderInfo) {
      this.pollableDataHelpers_.prerenderInfo.update(prerenderInfo);
    },

    receivedHttpPipeliningStatus: function(httpPipeliningStatus) {
      this.pollableDataHelpers_.httpPipeliningStatus.update(
          httpPipeliningStatus);
    },

    //--------------------------------------------------------------------------

    /**
     * Prevents receiving/sending events to/from the browser.
     */
    disable: function() {
      this.disabled_ = true;
      this.setPollInterval(0);
    },

    /**
     * Returns true if the BrowserBridge has been disabled.
     */
    isDisabled: function() {
      return this.disabled_;
    },

    /**
     * Adds a listener of the proxy settings. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onProxySettingsChanged(proxySettings)
     *
     * |proxySettings| is a dictionary with (up to) two properties:
     *
     *   "original"  -- The settings that chrome was configured to use
     *                  (i.e. system settings.)
     *   "effective" -- The "effective" proxy settings that chrome is using.
     *                  (decides between the manual/automatic modes of the
     *                  fetched settings).
     *
     * Each of these two configurations is formatted as a string, and may be
     * omitted if not yet initialized.
     *
     * If |ignoreWhenUnchanged| is true, data is only sent when it changes.
     * If it's false, data is sent whenever it's received from the browser.
     */
    addProxySettingsObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.proxySettings.addObserver(observer,
                                                          ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the proxy settings. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onBadProxiesChanged(badProxies)
     *
     * |badProxies| is an array, where each entry has the property:
     *   badProxies[i].proxy_uri: String identify the proxy.
     *   badProxies[i].bad_until: The time when the proxy stops being considered
     *                            bad. Note the time is in time ticks.
     */
    addBadProxiesObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.badProxies.addObserver(observer,
                                                       ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the host resolver info. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onHostResolverInfoChanged(hostResolverInfo)
     */
    addHostResolverInfoObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.hostResolverInfo.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the socket pool. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onSocketPoolInfoChanged(socketPoolInfo)
     */
    addSocketPoolInfoObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.socketPoolInfo.addObserver(observer,
                                                           ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the network session. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onSessionNetworkStatsChanged(sessionNetworkStats)
     */
    addSessionNetworkStatsObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.sessionNetworkStats.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of persistent network session data. |observer| will be
     * called back when data is received, through:
     *
     *   observer.onHistoricNetworkStatsChanged(historicNetworkStats)
     */
    addHistoricNetworkStatsObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.historicNetworkStats.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the SPDY info. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onSpdySessionInfoChanged(spdySessionInfo)
     */
    addSpdySessionInfoObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.spdySessionInfo.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the SPDY status. |observer| will be called back
     * when data is received, through:
     *
     *   observer.onSpdyStatusChanged(spdyStatus)
     */
    addSpdyStatusObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.spdyStatus.addObserver(observer,
                                                       ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the AlternateProtocolMappings. |observer| will be
     * called back when data is received, through:
     *
     *   observer.onSpdyAlternateProtocolMappingsChanged(
     *       spdyAlternateProtocolMappings)
     */
    addSpdyAlternateProtocolMappingsObserver: function(observer,
                                                       ignoreWhenUnchanged) {
      this.pollableDataHelpers_.spdyAlternateProtocolMappings.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of the service providers info. |observer| will be called
     * back when data is received, through:
     *
     *   observer.onServiceProvidersChanged(serviceProviders)
     *
     * Will do nothing if on a platform other than Windows, as service providers
     * are only present on Windows.
     */
    addServiceProvidersObserver: function(observer, ignoreWhenUnchanged) {
      if (this.pollableDataHelpers_.serviceProviders) {
        this.pollableDataHelpers_.serviceProviders.addObserver(
            observer, ignoreWhenUnchanged);
      }
    },

    /**
     * Adds a listener for the progress of the connection tests.
     * The observer will be called back with:
     *
     *   observer.onStartedConnectionTestSuite();
     *   observer.onStartedConnectionTestExperiment(experiment);
     *   observer.onCompletedConnectionTestExperiment(experiment, result);
     *   observer.onCompletedConnectionTestSuite();
     */
    addConnectionTestsObserver: function(observer) {
      this.connectionTestsObservers_.push(observer);
    },

    /**
     * Adds a listener for the http cache info results.
     * The observer will be called back with:
     *
     *   observer.onHttpCacheInfoChanged(info);
     */
    addHttpCacheInfoObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.httpCacheInfo.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener for the results of HSTS (HTTPS Strict Transport Security)
     * queries. The observer will be called back with:
     *
     *   observer.onHSTSQueryResult(result);
     */
    addHSTSObserver: function(observer) {
      this.hstsObservers_.push(observer);
    },

    /**
     * Adds a listener for ONC file parse status. The observer will be called
     * back with:
     *
     *   observer.onONCFileParse(error);
     */
    addCrosONCFileParseObserver: function(observer) {
      this.crosONCFileParseObservers_.push(observer);
    },

    /**
     * Adds a listener for storing log file status. The observer will be called
     * back with:
     *
     *   observer.onStoreDebugLogs(status);
     */
    addStoreDebugLogsObserver: function(observer) {
      this.storeDebugLogsObservers_.push(observer);
    },

    /**
     * Adds a listener for network debugging mode status. The observer
     * will be called back with:
     *
     *   observer.onSetNetworkDebugMode(status);
     */
    addSetNetworkDebugModeObserver: function(observer) {
      this.setNetworkDebugModeObservers_.push(observer);
    },

    /**
     * Adds a listener for the received constants event. |observer| will be
     * called back when the constants are received, through:
     *
     *   observer.onReceivedConstants(constants);
     */
    addConstantsObserver: function(observer) {
      this.constantsObservers_.push(observer);
    },

    /**
     * Adds a listener for updated prerender info events
     * |observer| will be called back with:
     *
     *   observer.onPrerenderInfoChanged(prerenderInfo);
     */
    addPrerenderInfoObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.prerenderInfo.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * Adds a listener of HTTP pipelining status. |observer| will be called
     * back when data is received, through:
     *
     *   observer.onHttpPipelineStatusChanged(httpPipeliningStatus)
     */
    addHttpPipeliningStatusObserver: function(observer, ignoreWhenUnchanged) {
      this.pollableDataHelpers_.httpPipeliningStatus.addObserver(
          observer, ignoreWhenUnchanged);
    },

    /**
     * If |force| is true, calls all startUpdate functions.  Otherwise, just
     * runs updates with active observers.
     */
    checkForUpdatedInfo: function(force) {
      for (var name in this.pollableDataHelpers_) {
        var helper = this.pollableDataHelpers_[name];
        if (force || helper.hasActiveObserver())
          helper.startUpdate();
      }
    },

    /**
     * Calls all startUpdate functions and, if |callback| is non-null,
     * calls it with the results of all updates.
     */
    updateAllInfo: function(callback) {
      if (callback)
        new UpdateAllObserver(callback, this.pollableDataHelpers_);
      this.checkForUpdatedInfo(true);
    }
  };

  /**
   * This is a helper class used by BrowserBridge, to keep track of:
   *   - the list of observers interested in some piece of data.
   *   - the last known value of that piece of data.
   *   - the name of the callback method to invoke on observers.
   *   - the update function.
   * @constructor
   */
  function PollableDataHelper(observerMethodName, startUpdateFunction) {
    this.observerMethodName_ = observerMethodName;
    this.startUpdate = startUpdateFunction;
    this.observerInfos_ = [];
  }

  PollableDataHelper.prototype = {
    getObserverMethodName: function() {
      return this.observerMethodName_;
    },

    isObserver: function(object) {
      for (var i = 0; i < this.observerInfos_.length; i++) {
        if (this.observerInfos_[i].observer === object)
          return true;
      }
      return false;
    },

    /**
     * If |ignoreWhenUnchanged| is true, we won't send data again until it
     * changes.
     */
    addObserver: function(observer, ignoreWhenUnchanged) {
      this.observerInfos_.push(new ObserverInfo(observer, ignoreWhenUnchanged));
    },

    removeObserver: function(observer) {
      for (var i = 0; i < this.observerInfos_.length; i++) {
        if (this.observerInfos_[i].observer === observer) {
          this.observerInfos_.splice(i, 1);
          return;
        }
      }
    },

    /**
     * Helper function to handle calling all the observers, but ONLY if the data
     * has actually changed since last time or the observer has yet to receive
     * any data. This is used for data we received from browser on an update
     * loop.
     */
    update: function(data) {
      var prevData = this.currentData_;
      var changed = false;

      // If the data hasn't changed since last time, will only need to notify
      // observers that have not yet received any data.
      if (!prevData || JSON.stringify(prevData) != JSON.stringify(data)) {
        changed = true;
        this.currentData_ = data;
      }

      // Notify the observers of the change, as needed.
      for (var i = 0; i < this.observerInfos_.length; i++) {
        var observerInfo = this.observerInfos_[i];
        if (changed || !observerInfo.hasReceivedData ||
            !observerInfo.ignoreWhenUnchanged) {
          observerInfo.observer[this.observerMethodName_](this.currentData_);
          observerInfo.hasReceivedData = true;
        }
      }
    },

    /**
     * Returns true if one of the observers actively wants the data
     * (i.e. is visible).
     */
    hasActiveObserver: function() {
      for (var i = 0; i < this.observerInfos_.length; i++) {
        if (this.observerInfos_[i].observer.isActive())
          return true;
      }
      return false;
    }
  };

  /**
   * This is a helper class used by PollableDataHelper, to keep track of
   * each observer and whether or not it has received any data.  The
   * latter is used to make sure that new observers get sent data on the
   * update following their creation.
   * @constructor
   */
  function ObserverInfo(observer, ignoreWhenUnchanged) {
    this.observer = observer;
    this.hasReceivedData = false;
    this.ignoreWhenUnchanged = ignoreWhenUnchanged;
  }

  /**
   * This is a helper class used by BrowserBridge to send data to
   * a callback once data from all polls has been received.
   *
   * It works by keeping track of how many polling functions have
   * yet to receive data, and recording the data as it it received.
   *
   * @constructor
   */
  function UpdateAllObserver(callback, pollableDataHelpers) {
    this.callback_ = callback;
    this.observingCount_ = 0;
    this.updatedData_ = {};

    for (var name in pollableDataHelpers) {
      ++this.observingCount_;
      var helper = pollableDataHelpers[name];
      helper.addObserver(this);
      this[helper.getObserverMethodName()] =
          this.onDataReceived_.bind(this, helper, name);
    }
  }

  UpdateAllObserver.prototype = {
    isActive: function() {
      return true;
    },

    onDataReceived_: function(helper, name, data) {
      helper.removeObserver(this);
      --this.observingCount_;
      this.updatedData_[name] = data;
      if (this.observingCount_ == 0)
        this.callback_(this.updatedData_);
    }
  };

  return BrowserBridge;
})();
