/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |
//   Page O------- Frame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader O-------- WebFrame (via FrameLoaderClient)
//
// FrameLoader and Frame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// WebFrame is refcounted and has one ref on behalf of the FrameLoader/Frame.
// This is not a normal reference counted pointer because that would require
// changing WebKit code that we don't control. Instead, it is created with this
// ref initially and it is removed when the FrameLoader is getting destroyed.
//
// WebFrames are created in two places, first in WebViewImpl when the root
// frame is created, and second in WebFrame::CreateChildFrame when sub-frames
// are created. WebKit will hook up this object to the FrameLoader/Frame
// and the refcount will be correct.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe.
//
// Frame going away causes the FrameLoader to get deleted. In FrameLoader's
// destructor, it notifies its client with frameLoaderDestroyed. This calls
// WebFrame::Closing and then derefs the WebFrame and will cause it to be
// deleted (unless an external someone is also holding a reference).

#include "config.h"
#include "WebFrameImpl.h"

#include "AssociatedURLLoader.h"
#include "AsyncFileSystem.h"
#include "AsyncFileSystemChromium.h"
#include "BackForwardController.h"
#include "Chrome.h"
#include "ClipboardUtilitiesChromium.h"
#include "Console.h"
#include "DOMFileSystem.h"
#include "DOMUtilitiesPrivate.h"
#include "DOMWindow.h"
#include "DOMWindowIntents.h"
#include "DOMWrapperWorld.h"
#include "DeliveredIntent.h"
#include "DeliveredIntentClientImpl.h"
#include "DirectoryEntry.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentMarker.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventListenerWrapper.h"
#include "FileEntry.h"
#include "FileSystemType.h"
#include "FindInPageCoordinates.h"
#include "FocusController.h"
#include "FontCache.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
#include "IconURL.h"
#include "InspectorController.h"
#include "KURL.h"
#include "MessagePort.h"
#include "Node.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PageOverlay.h"
#include "Performance.h"
#include "PlatformMessagePortChannel.h"
#include "PluginDocument.h"
#include "PrintContext.h"
#include "RenderBox.h"
#include "RenderFrame.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "SchemeRegistry.h"
#include "ScriptCallStack.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "ScrollTypes.h"
#include "ScrollbarTheme.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SkiaUtils.h"
#include "SpellChecker.h"
#include "StyleInheritedData.h"
#include "SubstituteData.h"
#include "TextAffinity.h"
#include "TextIterator.h"
#include "TraceEvent.h"
#include "UserGestureIndicator.h"
#if USE(V8)
#include "V8Binding.h"
#include "V8DOMFileSystem.h"
#include "V8DirectoryEntry.h"
#include "V8FileEntry.h"
#include "V8GCController.h"
#endif
#include "WebAnimationControllerImpl.h"
#include "WebConsoleMessage.h"
#include "WebDOMEvent.h"
#include "WebDOMEventListener.h"
#include "WebDataSourceImpl.h"
#include "WebDeliveredIntentClient.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebHistoryItem.h"
#include "WebIconURL.h"
#include "WebInputElement.h"
#include "WebIntent.h"
#include "WebNode.h"
#include "WebPerformance.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPrintParams.h"
#include "WebRange.h"
#include "WebScriptSource.h"
#include "WebSecurityOrigin.h"
#include "WebViewImpl.h"
#include "XPathResult.h"
#include "htmlediting.h"
#include "markup.h"
#include "painting/GraphicsContextBuilder.h"
#include "platform/WebSerializedScriptValue.h"
#include <algorithm>
#include <public/Platform.h>
#include <public/WebFileSystem.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <public/WebSize.h>
#include <public/WebURLError.h>
#include <public/WebVector.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>

#if !WEBKIT_USING_V8
#include "APICast.h"
#include "JSBase.h"
#endif
using namespace WebCore;

namespace WebKit {

static int frameCount = 0;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char* const webFrameActiveCount = "WebFrameActiveCount";

// Backend for contentAsPlainText, this is a recursive function that gets
// the text for the current frame and all of its subframes. It will append
// the text of each frame in turn to the |output| up to |maxChars| length.
//
// The |frame| must be non-null.
//
// FIXME: We should use StringBuilder rather than Vector<UChar>.
static void frameContentAsPlainText(size_t maxChars, Frame* frame, Vector<UChar>* output)
{
    Document* document = frame->document();
    if (!document)
        return;

    if (!frame->view())
        return;

    // TextIterator iterates over the visual representation of the DOM. As such,
    // it requires you to do a layout before using it (otherwise it'll crash).
    if (frame->view()->needsLayout())
        frame->view()->layout();

    // Select the document body.
    RefPtr<Range> range(document->createRange());
    ExceptionCode exception = 0;
    range->selectNodeContents(document->body(), exception);

    if (!exception) {
        // The text iterator will walk nodes giving us text. This is similar to
        // the plainText() function in TextIterator.h, but we implement the maximum
        // size and also copy the results directly into a wstring, avoiding the
        // string conversion.
        for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
            const UChar* chars = it.characters();
            if (!chars) {
                if (it.length()) {
                    // It appears from crash reports that an iterator can get into a state
                    // where the character count is nonempty but the character pointer is
                    // null. advance()ing it will then just add that many to the null
                    // pointer which won't be caught in a null check but will crash.
                    //
                    // A null pointer and 0 length is common for some nodes.
                    //
                    // IF YOU CATCH THIS IN A DEBUGGER please let brettw know. We don't
                    // currently understand the conditions for this to occur. Ideally, the
                    // iterators would never get into the condition so we should fix them
                    // if we can.
                    ASSERT_NOT_REACHED();
                    break;
                }

                // Just got a null node, we can forge ahead!
                continue;
            }
            size_t toAppend =
                std::min(static_cast<size_t>(it.length()), maxChars - output->size());
            output->append(chars, toAppend);
            if (output->size() >= maxChars)
                return; // Filled up the buffer.
        }
    }

    // The separator between frames when the frames are converted to plain text.
    const UChar frameSeparator[] = { '\n', '\n' };
    const size_t frameSeparatorLen = 2;

    // Recursively walk the children.
    FrameTree* frameTree = frame->tree();
    for (Frame* curChild = frameTree->firstChild(); curChild; curChild = curChild->tree()->nextSibling()) {
        // Ignore the text of non-visible frames.
        RenderView* contentRenderer = curChild->contentRenderer();
        RenderPart* ownerRenderer = curChild->ownerRenderer();
        if (!contentRenderer || !contentRenderer->width() || !contentRenderer->height()
            || (contentRenderer->x() + contentRenderer->width() <= 0) || (contentRenderer->y() + contentRenderer->height() <= 0)
            || (ownerRenderer && ownerRenderer->style() && ownerRenderer->style()->visibility() != VISIBLE)) {
            continue;
        }

        // Make sure the frame separator won't fill up the buffer, and give up if
        // it will. The danger is if the separator will make the buffer longer than
        // maxChars. This will cause the computation above:
        //   maxChars - output->size()
        // to be a negative number which will crash when the subframe is added.
        if (output->size() >= maxChars - frameSeparatorLen)
            return;

        output->append(frameSeparator, frameSeparatorLen);
        frameContentAsPlainText(maxChars, curChild, output);
        if (output->size() >= maxChars)
            return; // Filled up the buffer.
    }
}

static long long generateFrameIdentifier()
{
    static long long next = 0;
    return ++next;
}

WebPluginContainerImpl* WebFrameImpl::pluginContainerFromFrame(Frame* frame)
{
    if (!frame)
        return 0;
    if (!frame->document() || !frame->document()->isPluginDocument())
        return 0;
    PluginDocument* pluginDocument = static_cast<PluginDocument*>(frame->document());
    return static_cast<WebPluginContainerImpl *>(pluginDocument->pluginWidget());
}

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overridden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext {
    WTF_MAKE_NONCOPYABLE(ChromePrintContext);
public:
    ChromePrintContext(Frame* frame)
        : PrintContext(frame)
        , m_printedPageWidth(0)
    {
    }

    virtual ~ChromePrintContext() { }

    virtual void begin(float width, float height)
    {
        ASSERT(!m_printedPageWidth);
        m_printedPageWidth = width;
        PrintContext::begin(m_printedPageWidth, height);
    }

    virtual void end()
    {
        PrintContext::end();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        IntRect pageRect = m_pageRects[pageNumber];
        return m_printedPageWidth / pageRect.width();
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
    // do the scaling and ignore the return value.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        IntRect pageRect = m_pageRects[pageNumber];
        float scale = m_printedPageWidth / pageRect.width();

        context.save();
#if OS(UNIX) && !OS(DARWIN)
        context.scale(WebCore::FloatSize(scale, scale));
#endif
        context.translate(static_cast<float>(-pageRect.x()), static_cast<float>(-pageRect.y()));
        context.clip(pageRect);
        frame()->view()->paintContents(&context, pageRect);
        context.restore();
        return scale;
    }

    void spoolAllPagesWithBoundaries(GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
    {
        if (!frame()->document() || !frame()->view() || !frame()->document()->renderer())
            return;

        frame()->document()->updateLayout();

        float pageHeight;
        computePageRects(FloatRect(FloatPoint(0, 0), pageSizeInPixels), 0, 0, 1, pageHeight);

        const float pageWidth = pageSizeInPixels.width();
        size_t numPages = pageRects().size();
        int totalHeight = numPages * (pageSizeInPixels.height() + 1) - 1;

        // Fill the whole background by white.
        graphicsContext.setFillColor(Color(255, 255, 255), ColorSpaceDeviceRGB);
        graphicsContext.fillRect(FloatRect(0, 0, pageWidth, totalHeight));

        graphicsContext.save();

        int currentHeight = 0;
        for (size_t pageIndex = 0; pageIndex < numPages; pageIndex++) {
            // Draw a line for a page boundary if this isn't the first page.
            if (pageIndex > 0) {
                graphicsContext.save();
                graphicsContext.setStrokeColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
                graphicsContext.setFillColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
                graphicsContext.drawLine(IntPoint(0, currentHeight), IntPoint(pageWidth, currentHeight));
                graphicsContext.restore();
            }

            graphicsContext.save();

            graphicsContext.translate(0, currentHeight);
#if !OS(UNIX) || OS(DARWIN)
            // Account for the disabling of scaling in spoolPage. In the context
            // of spoolAllPagesWithBoundaries the scale HAS NOT been pre-applied.
            float scale = getPageShrink(pageIndex);
            graphicsContext.scale(WebCore::FloatSize(scale, scale));
#endif
            spoolPage(graphicsContext, pageIndex);
            graphicsContext.restore();

            currentHeight += pageSizeInPixels.height() + 1;
        }

        graphicsContext.restore();
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        PrintContext::computePageRects(printRect, headerHeight, footerHeight, userScaleFactor, outPageHeight);
    }

    virtual int pageCount() const
    {
        return PrintContext::pageCount();
    }

    virtual bool shouldUseBrowserOverlays() const
    {
        return true;
    }

private:
    // Set when printing.
    float m_printedPageWidth;
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext : public ChromePrintContext {
public:
    ChromePluginPrintContext(Frame* frame, WebPluginContainerImpl* plugin, const WebPrintParams& printParams)
        : ChromePrintContext(frame), m_plugin(plugin), m_pageCount(0), m_printParams(printParams)
    {
    }

    virtual ~ChromePluginPrintContext() { }

    virtual void begin(float width, float height)
    {
    }

    virtual void end()
    {
        m_plugin->printEnd();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        // We don't shrink the page (maybe we should ask the widget ??)
        return 1.0;
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        m_printParams.printContentArea = IntRect(printRect);
        m_pageCount = m_plugin->printBegin(m_printParams);
    }

    virtual int pageCount() const
    {
        return m_pageCount;
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        m_plugin->printPage(pageNumber, &context);
        return 1.0;
    }

    virtual bool shouldUseBrowserOverlays() const
    {
        return false;
    }

private:
    // Set when printing.
    WebPluginContainerImpl* m_plugin;
    int m_pageCount;
    WebPrintParams m_printParams;

};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader)
{
    return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : 0;
}

WebFrameImpl::FindMatch::FindMatch(PassRefPtr<Range> range, int ordinal)
    : m_range(range)
    , m_ordinal(ordinal)
{
}

class WebFrameImpl::DeferredScopeStringMatches {
public:
    DeferredScopeStringMatches(WebFrameImpl* webFrame, int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
        : m_timer(this, &DeferredScopeStringMatches::doTimeout)
        , m_webFrame(webFrame)
        , m_identifier(identifier)
        , m_searchText(searchText)
        , m_options(options)
        , m_reset(reset)
    {
        m_timer.startOneShot(0.0);
    }

private:
    void doTimeout(Timer<DeferredScopeStringMatches>*)
    {
        m_webFrame->callScopeStringMatches(this, m_identifier, m_searchText, m_options, m_reset);
    }

    Timer<DeferredScopeStringMatches> m_timer;
    RefPtr<WebFrameImpl> m_webFrame;
    int m_identifier;
    WebString m_searchText;
    WebFindOptions m_options;
    bool m_reset;
};

// WebFrame -------------------------------------------------------------------

int WebFrame::instanceCount()
{
    return frameCount;
}

WebFrame* WebFrame::frameForCurrentContext()
{
#if USE(V8)
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    return frameForContext(context);
#else
    return NULL;
#endif
}

#if WEBKIT_USING_V8
WebFrame* WebFrame::frameForContext(v8::Handle<v8::Context> context)
{ 
   return WebFrameImpl::fromFrame(toFrameIfNotDetached(context));
}
#endif

WebFrame* WebFrame::fromFrameOwnerElement(const WebElement& element)
{
    return WebFrameImpl::fromFrameOwnerElement(PassRefPtr<Element>(element).get());
}

WebString WebFrameImpl::uniqueName() const
{
    return frame()->tree()->uniqueName();
}

WebString WebFrameImpl::assignedName() const
{
    return frame()->tree()->name();
}

void WebFrameImpl::setName(const WebString& name)
{
    frame()->tree()->setName(name);
}

long long WebFrameImpl::identifier() const
{
    return m_identifier;
}

WebVector<WebIconURL> WebFrameImpl::iconURLs(int iconTypes) const
{
    // The URL to the icon may be in the header. As such, only
    // ask the loader for the icon if it's finished loading.
    if (frame()->loader()->state() == FrameStateComplete)
        return frame()->loader()->icon()->urlsForTypes(iconTypes);
    return WebVector<WebIconURL>();
}

WebSize WebFrameImpl::scrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return view->scrollOffset();
}

WebSize WebFrameImpl::minimumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return view->minimumScrollPosition() - IntPoint();
}

WebSize WebFrameImpl::maximumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return view->maximumScrollPosition() - IntPoint();
}

void WebFrameImpl::setScrollOffset(const WebSize& offset)
{
    if (FrameView* view = frameView())
        view->setScrollOffset(IntPoint(offset.width, offset.height));
}

WebSize WebFrameImpl::contentsSize() const
{
    return frame()->view()->contentsSize();
}

int WebFrameImpl::contentsPreferredWidth() const
{
    if (frame()->document() && frame()->document()->renderView()) {
        FontCachePurgePreventer fontCachePurgePreventer;
        return frame()->document()->renderView()->minPreferredLogicalWidth();
    }
    return 0;
}

int WebFrameImpl::documentElementScrollHeight() const
{
    if (frame()->document() && frame()->document()->documentElement())
        return frame()->document()->documentElement()->scrollHeight();
    return 0;
}

bool WebFrameImpl::hasVisibleContent() const
{
    return frame()->view()->visibleWidth() > 0 && frame()->view()->visibleHeight() > 0;
}

bool WebFrameImpl::hasHorizontalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->horizontalScrollbar();
}

bool WebFrameImpl::hasVerticalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->verticalScrollbar();
}

WebView* WebFrameImpl::view() const
{
    return viewImpl();
}

WebFrame* WebFrameImpl::opener() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->loader()->opener());
}

void WebFrameImpl::setOpener(const WebFrame* webFrame)
{
    frame()->loader()->setOpener(webFrame ? static_cast<const WebFrameImpl*>(webFrame)->frame() : 0);
}

WebFrame* WebFrameImpl::parent() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->parent());
}

WebFrame* WebFrameImpl::top() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->top());
}

WebFrame* WebFrameImpl::firstChild() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->firstChild());
}

WebFrame* WebFrameImpl::lastChild() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->lastChild());
}

WebFrame* WebFrameImpl::nextSibling() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->nextSibling());
}

WebFrame* WebFrameImpl::previousSibling() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->previousSibling());
}

WebFrame* WebFrameImpl::traverseNext(bool wrap) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->traverseNextWithWrap(wrap));
}

WebFrame* WebFrameImpl::traversePrevious(bool wrap) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->traversePreviousWithWrap(wrap));
}

WebFrame* WebFrameImpl::findChildByName(const WebString& name) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->child(name));
}

#if ENABLE(XPATH)
WebFrame* WebFrameImpl::findChildByExpression(const WebString& xpath) const
{
    if (xpath.isEmpty())
        return 0;

    Document* document = frame()->document();

    ExceptionCode ec = 0;
    RefPtr<XPathResult> xpathResult = document->evaluate(xpath, document, 0, XPathResult::ORDERED_NODE_ITERATOR_TYPE, 0, ec);
    if (!xpathResult)
        return 0;

    Node* node = xpathResult->iterateNext(ec);
    if (!node || !node->isFrameOwnerElement())
        return 0;
    HTMLFrameOwnerElement* frameElement = static_cast<HTMLFrameOwnerElement*>(node);
    return fromFrame(frameElement->contentFrame());
}
#endif

WebDocument WebFrameImpl::document() const
{
    if (!frame() || !frame()->document())
        return WebDocument();
    return WebDocument(frame()->document());
}

WebAnimationController* WebFrameImpl::animationController()
{
    return &m_animationController;
}

#if ENABLE(PERFORMANCE_TIMELINE)
WebPerformance WebFrameImpl::performance() const
{
    if (!frame())
        return WebPerformance();
    return WebPerformance(frame()->document()->domWindow()->performance());
}
#endif

NPObject* WebFrameImpl::windowObject() const
{
    if (!frame())
        return 0;
    return frame()->script()->windowScriptNPObject();
}

void WebFrameImpl::bindToWindowObject(const WebString& name, NPObject* object)
{
    if (!frame() || !frame()->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;
#if USE(V8)
    frame()->script()->bindToWindowObject(frame(), String(name), object);
#else
    notImplemented();
#endif
}

void WebFrameImpl::executeScript(const WebScriptSource& source)
{
    ASSERT(frame());
    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    frame()->script()->executeScript(ScriptSourceCode(source.code, source.url, position));
}

#if WEBKIT_USING_V8
void WebFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup)
{
    ASSERT(frame());

    Vector<ScriptSourceCode> sources;
    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    frame()->script()->evaluateInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

void WebFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin& securityOrigin)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldSecurityOrigin(worldID, securityOrigin.get());
}

void WebFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString& policy)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(worldID, policy);
}
#endif

void WebFrameImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    ASSERT(frame());

    MessageLevel webCoreMessageLevel;
    switch (message.level) {
    case WebConsoleMessage::LevelTip:
        webCoreMessageLevel = TipMessageLevel;
        break;
    case WebConsoleMessage::LevelLog:
        webCoreMessageLevel = LogMessageLevel;
        break;
    case WebConsoleMessage::LevelWarning:
        webCoreMessageLevel = WarningMessageLevel;
        break;
    case WebConsoleMessage::LevelError:
        webCoreMessageLevel = ErrorMessageLevel;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    frame()->document()->addConsoleMessage(OtherMessageSource, webCoreMessageLevel, message.text);
}

void WebFrameImpl::collectGarbage()
{
    if (!frame())
        return;
    if (!frame()->settings()->isScriptEnabled())
        return;
#if WEBKIT_USING_V8
    V8GCController::collectGarbage();
#elif USE(JSC)
    WebCore::DOMWrapperWorld *world = mainThreadNormalWorld();
    JSC::ExecState *exec = m_frame->script()->globalObject(world)->globalExec();
    JSGarbageCollect(toRef(exec));
#else
    notImplemented();
#endif
}

bool WebFrameImpl::checkIfRunInsecureContent(const WebURL& url) const
{
    ASSERT(frame());
    return frame()->loader()->mixedContentChecker()->canRunInsecureContent(frame()->document()->securityOrigin(), url);
}

#if USE(JSC)
void WebFrameImpl::attachJSCDebugger(JSC::Debugger* debugger) {
  Page::setDebuggerForAllPages(debugger);
}
#endif

#if USE(V8)
v8::Handle<v8::Value> WebFrameImpl::executeScriptAndReturnValue(const WebScriptSource& source)
{
    ASSERT(frame());

    // FIXME: This fake user gesture is required to make a bunch of pyauto
    // tests pass. If this isn't needed in non-test situations, we should
    // consider removing this code and changing the tests.
    // http://code.google.com/p/chromium/issues/detail?id=86397
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    return frame()->script()->executeScript(ScriptSourceCode(source.code, source.url, position)).v8Value();
}

void WebFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup, WebVector<v8::Local<v8::Value> >* results)
{
    ASSERT(frame());

    Vector<ScriptSourceCode> sources;

    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    if (results) {
        Vector<ScriptValue> scriptResults;
        frame()->script()->evaluateInIsolatedWorld(worldID, sources, extensionGroup, &scriptResults);
        WebVector<v8::Local<v8::Value> > v8Results(scriptResults.size());
        for (unsigned i = 0; i < scriptResults.size(); i++)
            v8Results[i] = v8::Local<v8::Value>::New(scriptResults[i].v8Value());
        results->swap(v8Results);
    } else
        frame()->script()->evaluateInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

v8::Handle<v8::Value> WebFrameImpl::callFunctionEvenIfScriptDisabled(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> argv[])
{
    ASSERT(frame());
    return frame()->script()->callFunctionEvenIfScriptDisabled(function, receiver, argc, argv).v8Value();
}

v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContext() const
{
    if (!frame())
        return v8::Local<v8::Context>();
    return ScriptController::mainWorldContext(frame());
}

#if ENABLE(FILE_SYSTEM)
v8::Handle<v8::Value> WebFrameImpl::createFileSystem(WebFileSystem::Type type, const WebString& name, const WebString& path)
{
    ASSERT(frame());
    return toV8(DOMFileSystem::create(frame()->document(), name, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, path.utf8().data()), AsyncFileSystemChromium::create()));
}

v8::Handle<v8::Value> WebFrameImpl::createSerializableFileSystem(WebFileSystem::Type type, const WebString& name, const WebString& path)
{
    ASSERT(frame());
    RefPtr<DOMFileSystem> fileSystem = DOMFileSystem::create(frame()->document(), name, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, path.utf8().data()), AsyncFileSystemChromium::create());
    fileSystem->makeClonable();
    return toV8(fileSystem.release());
}

v8::Handle<v8::Value> WebFrameImpl::createFileEntry(WebFileSystem::Type type, const WebString& fileSystemName, const WebString& fileSystemPath, const WebString& filePath, bool isDirectory)
{
    ASSERT(frame());

    RefPtr<DOMFileSystemBase> fileSystem = DOMFileSystem::create(frame()->document(), fileSystemName, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, fileSystemPath.utf8().data()), AsyncFileSystemChromium::create());
    if (isDirectory)
        return toV8(DirectoryEntry::create(fileSystem, filePath));
    return toV8(FileEntry::create(fileSystem, filePath));
}
#endif
#endif // USE(V8)

void WebFrameImpl::reload(bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader()->history()->saveDocumentAndScrollState();
    frame()->loader()->reload(ignoreCache);
}

void WebFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader()->history()->saveDocumentAndScrollState();
    frame()->loader()->reloadWithOverrideURL(overrideUrl, ignoreCache);
}

void WebFrameImpl::loadRequest(const WebURLRequest& request)
{
    ASSERT(frame());
    ASSERT(!request.isNull());
    const ResourceRequest& resourceRequest = request.toResourceRequest();

#if USE(V8)
    if (resourceRequest.url().protocolIs("javascript")) {
        loadJavaScriptURL(resourceRequest.url());
        return;
    }
#endif

    frame()->loader()->load(FrameLoadRequest(frame(), resourceRequest));
}

void WebFrameImpl::loadHistoryItem(const WebHistoryItem& item)
{
    ASSERT(frame());
    RefPtr<HistoryItem> historyItem = PassRefPtr<HistoryItem>(item);
    ASSERT(historyItem);

    frame()->loader()->prepareForHistoryNavigation();
    RefPtr<HistoryItem> currentItem = frame()->loader()->history()->currentItem();
    m_inSameDocumentHistoryLoad = currentItem && currentItem->shouldDoSameDocumentNavigationTo(historyItem.get());
    frame()->page()->goToItem(historyItem.get(), FrameLoadTypeIndexedBackForward);
    m_inSameDocumentHistoryLoad = false;
}

void WebFrameImpl::loadData(const WebData& data, const WebString& mimeType, const WebString& textEncoding, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());

    // If we are loading substitute data to replace an existing load, then
    // inherit all of the properties of that original request.  This way,
    // reload will re-attempt the original request.  It is essential that
    // we only do this when there is an unreachableURL since a non-empty
    // unreachableURL informs FrameLoader::reload to load unreachableURL
    // instead of the currently loaded URL.
    ResourceRequest request;
    if (replace && !unreachableURL.isEmpty())
        request = frame()->loader()->originalRequest();
    request.setURL(baseURL);

    FrameLoadRequest frameRequest(frame(), request, SubstituteData(data, mimeType, textEncoding, unreachableURL));
    ASSERT(frameRequest.substituteData().isValid());
    frame()->loader()->load(frameRequest);
    if (replace) {
        // Do this to force WebKit to treat the load as replacing the currently
        // loaded page.
        frame()->loader()->setReplacing();
    }
}

void WebFrameImpl::loadHTMLString(const WebData& data, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());
    loadData(data, WebString::fromUTF8("text/html"), WebString::fromUTF8("UTF-8"), baseURL, unreachableURL, replace);
}

bool WebFrameImpl::isLoading() const
{
    if (!frame())
        return false;
    return frame()->loader()->isLoading();
}

void WebFrameImpl::stopLoading()
{
    if (!frame())
        return;
    // FIXME: Figure out what we should really do here.  It seems like a bug
    // that FrameLoader::stopLoading doesn't call stopAllLoaders.
    frame()->loader()->stopAllLoaders();
    frame()->loader()->stopLoading(UnloadEventPolicyNone);
}

WebDataSource* WebFrameImpl::provisionalDataSource() const
{
    ASSERT(frame());

    // We regard the policy document loader as still provisional.
    DocumentLoader* documentLoader = frame()->loader()->provisionalDocumentLoader();
    if (!documentLoader)
        documentLoader = frame()->loader()->policyDocumentLoader();

    return DataSourceForDocLoader(documentLoader);
}

WebDataSource* WebFrameImpl::dataSource() const
{
    ASSERT(frame());
    return DataSourceForDocLoader(frame()->loader()->documentLoader());
}

WebHistoryItem WebFrameImpl::previousHistoryItem() const
{
    ASSERT(frame());
    // We use the previous item here because documentState (filled-out forms)
    // only get saved to history when it becomes the previous item.  The caller
    // is expected to query the history item after a navigation occurs, after
    // the desired history item has become the previous entry.
    return WebHistoryItem(frame()->loader()->history()->previousItem());
}

WebHistoryItem WebFrameImpl::currentHistoryItem() const
{
    ASSERT(frame());

    // We're shutting down.
    if (!frame()->loader()->activeDocumentLoader())
        return WebHistoryItem();

    // If we are still loading, then we don't want to clobber the current
    // history item as this could cause us to lose the scroll position and
    // document state.  However, it is OK for new navigations.
    // FIXME: Can we make this a plain old getter, instead of worrying about
    // clobbering here?
    if (!m_inSameDocumentHistoryLoad && (frame()->loader()->loadType() == FrameLoadTypeStandard
        || !frame()->loader()->activeDocumentLoader()->isLoadingInAPISense()))
        frame()->loader()->history()->saveDocumentAndScrollState();

    return WebHistoryItem(frame()->page()->backForward()->currentItem());
}

void WebFrameImpl::enableViewSourceMode(bool enable)
{
    if (frame())
        frame()->setInViewSourceMode(enable);
}

bool WebFrameImpl::isViewSourceModeEnabled() const
{
    if (!frame())
        return false;
    return frame()->inViewSourceMode();
}

void WebFrameImpl::setReferrerForRequest(WebURLRequest& request, const WebURL& referrerURL)
{
    String referrer = referrerURL.isEmpty() ? frame()->loader()->outgoingReferrer() : String(referrerURL.spec().utf16());
    referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), request.url(), referrer);
    if (referrer.isEmpty())
        return;
    request.setHTTPHeaderField(WebString::fromUTF8("Referer"), referrer);
}

void WebFrameImpl::dispatchWillSendRequest(WebURLRequest& request)
{
    ResourceResponse response;
    frame()->loader()->client()->dispatchWillSendRequest(0, 0, request.toMutableResourceRequest(), response);
}

WebURLLoader* WebFrameImpl::createAssociatedURLLoader(const WebURLLoaderOptions& options)
{
    return new AssociatedURLLoader(this, options);
}

void WebFrameImpl::commitDocumentData(const char* data, size_t length)
{
    frame()->loader()->documentLoader()->commitData(data, length);
}

unsigned WebFrameImpl::unloadListenerCount() const
{
    return frame()->document()->domWindow()->pendingUnloadEventListeners();
}

bool WebFrameImpl::isProcessingUserGesture() const
{
    return ScriptController::processingUserGesture();
}

bool WebFrameImpl::consumeUserGesture() const
{
    return UserGestureIndicator::consumeUserGesture();
}

bool WebFrameImpl::willSuppressOpenerInNewFrame() const
{
    return frame()->loader()->suppressOpenerInNewFrame();
}

void WebFrameImpl::replaceSelection(const WebString& text)
{
    bool selectReplacement = false;
    bool smartReplace = true;
    frame()->editor()->replaceSelectionWithText(text, selectReplacement, smartReplace);
}

void WebFrameImpl::insertText(const WebString& text)
{
    if (frame()->editor()->hasComposition())
        frame()->editor()->confirmComposition(text);
    else
        frame()->editor()->insertText(text, 0);
}

void WebFrameImpl::setMarkedText(const WebString& text, unsigned location, unsigned length)
{
    Vector<CompositionUnderline> decorations;
    frame()->editor()->setComposition(text, decorations, location, length);
}

void WebFrameImpl::unmarkText()
{
    frame()->editor()->cancelComposition();
}

bool WebFrameImpl::hasMarkedText() const
{
    return frame()->editor()->hasComposition();
}

WebRange WebFrameImpl::markedRange() const
{
    return frame()->editor()->compositionRange();
}

bool WebFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect& rect) const
{
    if ((location + length < location) && (location + length))
        length = 0;

    RefPtr<Range> range = TextIterator::rangeFromLocationAndLength(frame()->selection()->rootEditableElementOrDocumentElement(), location, length);
    if (!range)
        return false;
    IntRect intRect = frame()->editor()->firstRectForRange(range.get());
    rect = WebRect(intRect);
    rect = frame()->view()->contentsToWindow(rect);
    return true;
}

size_t WebFrameImpl::characterIndexForPoint(const WebPoint& webPoint) const
{
    if (!frame())
        return notFound;

    IntPoint point = frame()->view()->windowToContents(webPoint);
    HitTestResult result = frame()->eventHandler()->hitTestResultAtPoint(point, false);
    RefPtr<Range> range = frame()->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (!range)
        return notFound;

    size_t location, length;
    TextIterator::getLocationAndLengthFromRange(frame()->selection()->rootEditableElementOrDocumentElement(), range.get(), location, length);
    return location;
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebNode& node)
{
    ASSERT(frame());

    if (name.length() <= 2)
        return false;

    // Since we don't have NSControl, we will convert the format of command
    // string and call the function on Editor directly.
    String command = name;

    // Make sure the first letter is upper case.
    command.replace(0, 1, command.substring(0, 1).upper());

    // Remove the trailing ':' if existing.
    if (command[command.length() - 1] == UChar(':'))
        command = command.substring(0, command.length() - 1);

    if (command == "Copy") {
        WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
        if (!pluginContainer)
            pluginContainer = static_cast<WebPluginContainerImpl*>(node.pluginContainer());
        if (pluginContainer) {
            pluginContainer->copy();
            return true;
        }
    }

    bool result = true;

    // Specially handling commands that Editor::execCommand does not directly
    // support.
    if (command == "DeleteToEndOfParagraph") {
        if (!frame()->editor()->deleteWithDirection(DirectionForward, ParagraphBoundary, true, false))
            frame()->editor()->deleteWithDirection(DirectionForward, CharacterGranularity, true, false);
    } else if (command == "Indent")
        frame()->editor()->indent();
    else if (command == "Outdent")
        frame()->editor()->outdent();
    else if (command == "DeleteBackward")
        result = frame()->editor()->command(AtomicString("BackwardDelete")).execute();
    else if (command == "DeleteForward")
        result = frame()->editor()->command(AtomicString("ForwardDelete")).execute();
    else if (command == "AdvanceToNextMisspelling") {
        // Wee need to pass false here or else the currently selected word will never be skipped.
        frame()->editor()->advanceToNextMisspelling(false);
    } else if (command == "ToggleSpellPanel")
        frame()->editor()->showSpellingGuessPanel();
    else
        result = frame()->editor()->command(command).execute();
    return result;
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebString& value)
{
    ASSERT(frame());
    String webName = name;

    // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit for editable nodes.
    if (!frame()->editor()->canEdit() && webName == "moveToBeginningOfDocument")
        return viewImpl()->propagateScroll(ScrollUp, ScrollByDocument);

    if (!frame()->editor()->canEdit() && webName == "moveToEndOfDocument")
        return viewImpl()->propagateScroll(ScrollDown, ScrollByDocument);

    return frame()->editor()->command(webName).execute(value);
}

bool WebFrameImpl::isCommandEnabled(const WebString& name) const
{
    ASSERT(frame());
    return frame()->editor()->command(name).isEnabled();
}

void WebFrameImpl::enableContinuousSpellChecking(bool enable)
{
    if (enable == isContinuousSpellCheckingEnabled())
        return;
    frame()->editor()->toggleContinuousSpellChecking();
}

bool WebFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return frame()->editor()->isContinuousSpellCheckingEnabled();
}

void WebFrameImpl::requestTextChecking(const WebElement& webElement)
{
    if (webElement.isNull())
        return;
    RefPtr<Range> rangeToCheck = rangeOfContents(const_cast<Element*>(webElement.constUnwrap<Element>()));
    frame()->editor()->spellChecker()->requestCheckingFor(SpellCheckRequest::create(TextCheckingTypeSpelling | TextCheckingTypeGrammar, TextCheckingProcessBatch, rangeToCheck, rangeToCheck));
}

void WebFrameImpl::replaceMisspelledRange(const WebString& text)
{
    // If this caret selection has two or more markers, this function replace the range covered by the first marker with the specified word as Microsoft Word does.
    if (pluginContainerFromFrame(frame()))
        return;
    RefPtr<Range> caretRange = frame()->selection()->toNormalizedRange();
    if (!caretRange)
        return;
    Vector<DocumentMarker*> markers = frame()->document()->markers()->markersInRange(caretRange.get(), DocumentMarker::Spelling | DocumentMarker::Grammar);
    if (markers.size() < 1 || markers[0]->startOffset() >= markers[0]->endOffset())
        return;
    RefPtr<Range> markerRange = TextIterator::rangeFromLocationAndLength(frame()->selection()->rootEditableElementOrDocumentElement(), markers[0]->startOffset(), markers[0]->endOffset() - markers[0]->startOffset());
    if (!markerRange.get() || !frame()->selection()->shouldChangeSelection(markerRange.get()))
        return;
    frame()->selection()->setSelection(markerRange.get(), CharacterGranularity);
    frame()->editor()->replaceSelectionWithText(text, false, true);
}

bool WebFrameImpl::hasSelection() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->hasSelection();

    // frame()->selection()->isNone() never returns true.
    return (frame()->selection()->start() != frame()->selection()->end());
}

WebRange WebFrameImpl::selectionRange() const
{
    return frame()->selection()->toNormalizedRange();
}

WebString WebFrameImpl::selectionAsText() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsText();

    RefPtr<Range> range = frame()->selection()->toNormalizedRange();
    if (!range)
        return WebString();

    String text = range->text();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
#if !defined(__LB_SHELL__)
    replaceNBSPWithSpace(text);
#endif
    return text;
}

WebString WebFrameImpl::selectionAsMarkup() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsMarkup();

    RefPtr<Range> range = frame()->selection()->toNormalizedRange();
    if (!range)
        return WebString();

    return createMarkup(range.get(), 0, AnnotateForInterchange, false, ResolveNonLocalURLs);
}

void WebFrameImpl::selectWordAroundPosition(Frame* frame, VisiblePosition position)
{
    VisibleSelection selection(position);
    selection.expandUsingGranularity(WordGranularity);

    if (frame->selection()->shouldChangeSelection(selection)) {
        TextGranularity granularity = selection.isRange() ? WordGranularity : CharacterGranularity;
        frame->selection()->setSelection(selection, granularity);
    }
}

bool WebFrameImpl::selectWordAroundCaret()
{
    FrameSelection* selection = frame()->selection();
    ASSERT(!selection->isNone());
    if (selection->isNone() || selection->isRange())
        return false;
    selectWordAroundPosition(frame(), selection->selection().visibleStart());
    return true;
}

void WebFrameImpl::selectRange(const WebPoint& base, const WebPoint& extent)
{
    VisiblePosition basePosition = visiblePositionForWindowPoint(base);
    VisiblePosition extentPosition = visiblePositionForWindowPoint(extent);
    VisibleSelection newSelection = VisibleSelection(basePosition, extentPosition);
    if (frame()->selection()->shouldChangeSelection(newSelection))
        frame()->selection()->setSelection(newSelection, CharacterGranularity);
}

void WebFrameImpl::selectRange(const WebRange& webRange)
{
    if (RefPtr<Range> range = static_cast<PassRefPtr<Range> >(webRange))
        frame()->selection()->setSelectedRange(range.get(), WebCore::VP_DEFAULT_AFFINITY, false);
}

VisiblePosition WebFrameImpl::visiblePositionForWindowPoint(const WebPoint& point)
{
    HitTestRequest request = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping;
    HitTestResult result(frame()->view()->windowToContents(IntPoint(point)));

    frame()->document()->renderView()->layer()->hitTest(request, result);

    Node* node = result.targetNode();
    if (!node)
        return VisiblePosition();
    return node->renderer()->positionForPoint(result.localPoint());
}

int WebFrameImpl::printBegin(const WebPrintParams& printParams, const WebNode& constrainToNode, bool* useBrowserOverlays)
{
    ASSERT(!frame()->document()->isFrameSet());
    WebPluginContainerImpl* pluginContainer = 0;
    if (constrainToNode.isNull()) {
        // If this is a plugin document, check if the plugin supports its own
        // printing. If it does, we will delegate all printing to that.
        pluginContainer = pluginContainerFromFrame(frame());
    } else {
        // We only support printing plugin nodes for now.
        pluginContainer = static_cast<WebPluginContainerImpl*>(constrainToNode.pluginContainer());
    }

    if (pluginContainer && pluginContainer->supportsPaginatedPrint())
        m_printContext = adoptPtr(new ChromePluginPrintContext(frame(), pluginContainer, printParams));
    else
        m_printContext = adoptPtr(new ChromePrintContext(frame()));

    FloatRect rect(0, 0, static_cast<float>(printParams.printContentArea.width), static_cast<float>(printParams.printContentArea.height));
    m_printContext->begin(rect.width(), rect.height());
    float pageHeight;
    // We ignore the overlays calculation for now since they are generated in the
    // browser. pageHeight is actually an output parameter.
    m_printContext->computePageRects(rect, 0, 0, 1.0, pageHeight);
    if (useBrowserOverlays)
        *useBrowserOverlays = m_printContext->shouldUseBrowserOverlays();

    return m_printContext->pageCount();
}

float WebFrameImpl::getPrintPageShrink(int page)
{
    ASSERT(m_printContext && page >= 0);
    return m_printContext->getPageShrink(page);
}

float WebFrameImpl::printPage(int page, WebCanvas* canvas)
{
#if ENABLE(PRINTING)
    ASSERT(m_printContext && page >= 0 && frame() && frame()->document());

    GraphicsContextBuilder builder(canvas);
    GraphicsContext& graphicsContext = builder.context();
    graphicsContext.platformContext()->setPrinting(true);
    return m_printContext->spoolPage(graphicsContext, page);
#else
    return 0;
#endif
}

void WebFrameImpl::printEnd()
{
    ASSERT(m_printContext);
    m_printContext->end();
    m_printContext.clear();
}

bool WebFrameImpl::isPrintScalingDisabledForPlugin(const WebNode& node)
{
    WebPluginContainerImpl* pluginContainer =  node.isNull() ? pluginContainerFromFrame(frame()) : static_cast<WebPluginContainerImpl*>(node.pluginContainer());

    if (!pluginContainer || !pluginContainer->supportsPaginatedPrint())
        return false;

    return pluginContainer->isPrintScalingDisabled();
}

bool WebFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    return frame()->document()->styleForPage(pageIndex)->pageSizeType() != PAGE_SIZE_AUTO;
}

bool WebFrameImpl::isPageBoxVisible(int pageIndex)
{
    return frame()->document()->isPageBoxVisible(pageIndex);
}

void WebFrameImpl::pageSizeAndMarginsInPixels(int pageIndex, WebSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    IntSize size = pageSize;
    frame()->document()->pageSizeAndMarginsInPixels(pageIndex, size, marginTop, marginRight, marginBottom, marginLeft);
    pageSize = size;
}

WebString WebFrameImpl::pageProperty(const WebString& propertyName, int pageIndex)
{
    ASSERT(m_printContext);
    return m_printContext->pageProperty(frame(), propertyName.utf8().data(), pageIndex);
}

bool WebFrameImpl::find(int identifier, const WebString& searchText, const WebFindOptions& options, bool wrapWithinFrame, WebRect* selectionRect)
{
    if (!frame() || !frame()->page())
        return false;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (!options.findNext)
        frame()->page()->unmarkAllTextMatches();
    else
        setMarkerActive(m_activeMatch.get(), false);

    if (m_activeMatch && m_activeMatch->ownerDocument() != frame()->document())
        m_activeMatch = 0;

    // If the user has selected something since the last Find operation we want
    // to start from there. Otherwise, we start searching from where the last Find
    // operation left off (either a Find or a FindNext operation).
    VisibleSelection selection(frame()->selection()->selection());
    bool activeSelection = !selection.isNone();
    if (activeSelection) {
        m_activeMatch = selection.firstRange().get();
        frame()->selection()->clear();
    }

    ASSERT(frame() && frame()->view());
    const FindOptions findOptions = (options.forward ? 0 : Backwards)
        | (options.matchCase ? 0 : CaseInsensitive)
        | (wrapWithinFrame ? WrapAround : 0)
        | (!options.findNext ? StartInSelection : 0);
    m_activeMatch = frame()->editor()->findStringAndScrollToVisible(searchText, m_activeMatch.get(), findOptions);

    if (!m_activeMatch) {
        // If we're finding next the next active match might not be in the current frame.
        // In this case we don't want to clear the matches cache.
        if (!options.findNext)
            clearFindMatchesCache();
        invalidateArea(InvalidateAll);
        return false;
    }

#if OS(ANDROID)
    viewImpl()->zoomToFindInPageRect(frameView()->contentsToWindow(enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(m_activeMatch.get()))));
#endif

    setMarkerActive(m_activeMatch.get(), true);
    WebFrameImpl* oldActiveFrame = mainFrameImpl->m_currentActiveMatchFrame;
    mainFrameImpl->m_currentActiveMatchFrame = this;

    // Make sure no node is focused. See http://crbug.com/38700.
    frame()->document()->setFocusedNode(0);

    if (!options.findNext || activeSelection) {
        // This is either a Find operation or a Find-next from a new start point
        // due to a selection, so we set the flag to ask the scoping effort
        // to find the active rect for us and report it back to the UI.
        m_locatingActiveRect = true;
    } else {
        if (oldActiveFrame != this) {
            if (options.forward)
                m_activeMatchIndexInCurrentFrame = 0;
            else
                m_activeMatchIndexInCurrentFrame = m_lastMatchCount - 1;
        } else {
            if (options.forward)
                ++m_activeMatchIndexInCurrentFrame;
            else
                --m_activeMatchIndexInCurrentFrame;

            if (m_activeMatchIndexInCurrentFrame + 1 > m_lastMatchCount)
                m_activeMatchIndexInCurrentFrame = 0;
            if (m_activeMatchIndexInCurrentFrame == -1)
                m_activeMatchIndexInCurrentFrame = m_lastMatchCount - 1;
        }
        if (selectionRect) {
            *selectionRect = frameView()->contentsToWindow(m_activeMatch->boundingBox());
            reportFindInPageSelection(*selectionRect, m_activeMatchIndexInCurrentFrame + 1, identifier);
        }
    }

    return true;
}

void WebFrameImpl::stopFinding(bool clearSelection)
{
    if (!clearSelection)
        setFindEndstateFocusAndSelection();
    cancelPendingScopingEffort();

    // Remove all markers for matches found and turn off the highlighting.
    frame()->document()->markers()->removeMarkers(DocumentMarker::TextMatch);
    frame()->editor()->setMarkedTextMatchesAreHighlighted(false);
    clearFindMatchesCache();

    // Let the frame know that we don't want tickmarks or highlighting anymore.
    invalidateArea(InvalidateAll);
}

void WebFrameImpl::scopeStringMatches(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    if (reset) {
        // This is a brand new search, so we need to reset everything.
        // Scoping is just about to begin.
        m_scopingInProgress = true;

        // Need to keep the current identifier locally in order to finish the
        // request in case the frame is detached during the process.
        m_findRequestIdentifier = identifier;

        // Clear highlighting for this frame.
        if (frame() && frame()->page() && frame()->editor()->markedTextMatchesAreHighlighted())
            frame()->page()->unmarkAllTextMatches();

        // Clear the tickmarks and results cache.
        clearFindMatchesCache();

        // Clear the counters from last operation.
        m_lastMatchCount = 0;
        m_nextInvalidateAfter = 0;

        m_resumeScopingFromRange = 0;

        // The view might be null on detached frames.
        if (frame() && frame()->page())
            viewImpl()->mainFrameImpl()->m_framesScopingCount++;

        // Now, defer scoping until later to allow find operation to finish quickly.
        scopeStringMatchesSoon(identifier, searchText, options, false); // false means just reset, so don't do it again.
        return;
    }

    if (!shouldScopeMatches(searchText)) {
        // Note that we want to defer the final update when resetting even if shouldScopeMatches returns false.
        // This is done in order to prevent sending a final message based only on the results of the first frame
        // since m_framesScopingCount would be 0 as other frames have yet to reset.
        finishCurrentScopingEffort(identifier);
        return;
    }

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    RefPtr<Range> searchRange(rangeOfContents(frame()->document()));

    Node* originalEndContainer = searchRange->endContainer();
    int originalEndOffset = searchRange->endOffset();

    ExceptionCode ec = 0, ec2 = 0;
    if (m_resumeScopingFromRange) {
        // This is a continuation of a scoping operation that timed out and didn't
        // complete last time around, so we should start from where we left off.
        searchRange->setStart(m_resumeScopingFromRange->startContainer(),
                              m_resumeScopingFromRange->startOffset(ec2) + 1,
                              ec);
        if (ec || ec2) {
            if (ec2) // A non-zero |ec| happens when navigating during search.
                ASSERT_NOT_REACHED();
            return;
        }
    }

    // This timeout controls how long we scope before releasing control.  This
    // value does not prevent us from running for longer than this, but it is
    // periodically checked to see if we have exceeded our allocated time.
    const double maxScopingDuration = 0.1; // seconds

    int matchCount = 0;
    bool timedOut = false;
    double startTime = currentTime();
    do {
        // Find next occurrence of the search string.
        // FIXME: (http://b/1088245) This WebKit operation may run for longer
        // than the timeout value, and is not interruptible as it is currently
        // written. We may need to rewrite it with interruptibility in mind, or
        // find an alternative.
        RefPtr<Range> resultRange(findPlainText(searchRange.get(),
                                                searchText,
                                                options.matchCase ? 0 : CaseInsensitive));
        if (resultRange->collapsed(ec)) {
            if (!resultRange->startContainer()->isInShadowTree())
                break;

            searchRange->setStartAfter(
                resultRange->startContainer()->shadowAncestorNode(), ec);
            searchRange->setEnd(originalEndContainer, originalEndOffset, ec);
            continue;
        }

        ++matchCount;

        // Catch a special case where Find found something but doesn't know what
        // the bounding box for it is. In this case we set the first match we find
        // as the active rect.
        IntRect resultBounds = resultRange->boundingBox();
        IntRect activeSelectionRect;
        if (m_locatingActiveRect) {
            activeSelectionRect = m_activeMatch.get() ?
                m_activeMatch->boundingBox() : resultBounds;
        }

        // If the Find function found a match it will have stored where the
        // match was found in m_activeSelectionRect on the current frame. If we
        // find this rect during scoping it means we have found the active
        // tickmark.
        bool foundActiveMatch = false;
        if (m_locatingActiveRect && (activeSelectionRect == resultBounds)) {
            // We have found the active tickmark frame.
            mainFrameImpl->m_currentActiveMatchFrame = this;
            foundActiveMatch = true;
            // We also know which tickmark is active now.
            m_activeMatchIndexInCurrentFrame = matchCount - 1;
            // To stop looking for the active tickmark, we set this flag.
            m_locatingActiveRect = false;

            // Notify browser of new location for the selected rectangle.
            reportFindInPageSelection(
                frameView()->contentsToWindow(resultBounds),
                m_activeMatchIndexInCurrentFrame + 1,
                identifier);
        }

        addMarker(resultRange.get(), foundActiveMatch);

        m_findMatchesCache.append(FindMatch(resultRange.get(), m_lastMatchCount + matchCount));

        // Set the new start for the search range to be the end of the previous
        // result range. There is no need to use a VisiblePosition here,
        // since findPlainText will use a TextIterator to go over the visible
        // text nodes.
        searchRange->setStart(resultRange->endContainer(ec), resultRange->endOffset(ec), ec);

        Node* shadowTreeRoot = searchRange->shadowRoot();
        if (searchRange->collapsed(ec) && shadowTreeRoot)
            searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), ec);

        m_resumeScopingFromRange = resultRange;
        timedOut = (currentTime() - startTime) >= maxScopingDuration;
    } while (!timedOut);

    // Remember what we search for last time, so we can skip searching if more
    // letters are added to the search string (and last outcome was 0).
    m_lastSearchString = searchText;

    if (matchCount > 0) {
        frame()->editor()->setMarkedTextMatchesAreHighlighted(true);

        m_lastMatchCount += matchCount;

        // Let the mainframe know how much we found during this pass.
        mainFrameImpl->increaseMatchCount(matchCount, identifier);
    }

    if (timedOut) {
        // If we found anything during this pass, we should redraw. However, we
        // don't want to spam too much if the page is extremely long, so if we
        // reach a certain point we start throttling the redraw requests.
        if (matchCount > 0)
            invalidateIfNecessary();

        // Scoping effort ran out of time, lets ask for another time-slice.
        scopeStringMatchesSoon(
            identifier,
            searchText,
            options,
            false); // don't reset.
        return; // Done for now, resume work later.
    }

    finishCurrentScopingEffort(identifier);
}

void WebFrameImpl::flushCurrentScopingEffort(int identifier)
{
    if (!frame() || !frame()->page())
        return;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    // This frame has no further scoping left, so it is done. Other frames might,
    // of course, continue to scope matches.
    mainFrameImpl->m_framesScopingCount--;

    // If this is the last frame to finish scoping we need to trigger the final
    // update to be sent.
    if (!mainFrameImpl->m_framesScopingCount)
        mainFrameImpl->increaseMatchCount(0, identifier);
}

void WebFrameImpl::finishCurrentScopingEffort(int identifier)
{
    flushCurrentScopingEffort(identifier);

    m_scopingInProgress = false;
    m_lastFindRequestCompletedWithNoMatches = !m_lastMatchCount;

    // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
    invalidateArea(InvalidateScrollbar);
}

void WebFrameImpl::cancelPendingScopingEffort()
{
    deleteAllValues(m_deferredScopingWork);
    m_deferredScopingWork.clear();

    m_activeMatchIndexInCurrentFrame = -1;

    // Last request didn't complete.
    if (m_scopingInProgress)
        m_lastFindRequestCompletedWithNoMatches = false;

    m_scopingInProgress = false;
}

void WebFrameImpl::increaseMatchCount(int count, int identifier)
{
    // This function should only be called on the mainframe.
    ASSERT(!parent());

    if (count)
        ++m_findMatchMarkersVersion;

    m_totalMatchCount += count;

    // Update the UI with the latest findings.
    if (client())
        client()->reportFindInPageMatchCount(identifier, m_totalMatchCount, !m_framesScopingCount);
}

void WebFrameImpl::reportFindInPageSelection(const WebRect& selectionRect, int activeMatchOrdinal, int identifier)
{
    // Update the UI with the latest selection rect.
    if (client())
        client()->reportFindInPageSelection(identifier, ordinalOfFirstMatchForFrame(this) + activeMatchOrdinal, selectionRect);
}

void WebFrameImpl::resetMatchCount()
{
    if (m_totalMatchCount > 0)
        ++m_findMatchMarkersVersion;

    m_totalMatchCount = 0;
    m_framesScopingCount = 0;
}

void WebFrameImpl::sendOrientationChangeEvent(int orientation)
{
#if ENABLE(ORIENTATION_EVENTS)
    if (frame())
        frame()->sendOrientationChangeEvent(orientation);
#endif
}

void WebFrameImpl::addEventListener(const WebString& eventType, WebDOMEventListener* listener, bool useCapture)
{
    DOMWindow* window = frame()->document()->domWindow();
    EventListenerWrapper* listenerWrapper = listener->createEventListenerWrapper(eventType, useCapture, window);
    window->addEventListener(eventType, adoptRef(listenerWrapper), useCapture);
}

void WebFrameImpl::removeEventListener(const WebString& eventType, WebDOMEventListener* listener, bool useCapture)
{
    DOMWindow* window = frame()->document()->domWindow();
    EventListenerWrapper* listenerWrapper = listener->getEventListenerWrapper(eventType, useCapture, window);
    window->removeEventListener(eventType, listenerWrapper, useCapture);
}

bool WebFrameImpl::dispatchEvent(const WebDOMEvent& event)
{
    ASSERT(!event.isNull());
    return frame()->document()->domWindow()->dispatchEvent(event);
}

void WebFrameImpl::dispatchMessageEventWithOriginCheck(const WebSecurityOrigin& intendedTargetOrigin, const WebDOMEvent& event)
{
    ASSERT(!event.isNull());
    frame()->document()->domWindow()->dispatchMessageEventWithOriginCheck(intendedTargetOrigin.get(), event, 0);
}

int WebFrameImpl::findMatchMarkersVersion() const
{
    ASSERT(!parent());
    return m_findMatchMarkersVersion;
}

void WebFrameImpl::clearFindMatchesCache()
{
    if (!m_findMatchesCache.isEmpty())
        viewImpl()->mainFrameImpl()->m_findMatchMarkersVersion++;

    m_findMatchesCache.clear();
    m_findMatchRectsAreValid = false;
}

bool WebFrameImpl::isActiveMatchFrameValid() const
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    WebFrameImpl* activeMatchFrame = mainFrameImpl->activeMatchFrame();
    return activeMatchFrame && activeMatchFrame->m_activeMatch && activeMatchFrame->frame()->tree()->isDescendantOf(mainFrameImpl->frame());
}

void WebFrameImpl::updateFindMatchRects()
{
    IntSize currentContentsSize = contentsSize();
    if (m_contentsSizeForCurrentFindMatchRects != currentContentsSize) {
        m_contentsSizeForCurrentFindMatchRects = currentContentsSize;
        m_findMatchRectsAreValid = false;
    }

    size_t deadMatches = 0;
    for (Vector<FindMatch>::iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it) {
        if (!it->m_range->boundaryPointsValid() || !it->m_range->startContainer()->inDocument())
            it->m_rect = FloatRect();
        else if (!m_findMatchRectsAreValid)
            it->m_rect = findInPageRectFromRange(it->m_range.get());

        if (it->m_rect.isEmpty())
            ++deadMatches;
    }

    // Remove any invalid matches from the cache.
    if (deadMatches) {
        Vector<FindMatch> filteredMatches;
        filteredMatches.reserveCapacity(m_findMatchesCache.size() - deadMatches);

        for (Vector<FindMatch>::const_iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it)
            if (!it->m_rect.isEmpty())
                filteredMatches.append(*it);

        m_findMatchesCache.swap(filteredMatches);
    }

    // Invalidate the rects in child frames. Will be updated later during traversal.
    if (!m_findMatchRectsAreValid)
        for (WebFrame* child = firstChild(); child; child = child->nextSibling())
            static_cast<WebFrameImpl*>(child)->m_findMatchRectsAreValid = false;

    m_findMatchRectsAreValid = true;
}

WebFloatRect WebFrameImpl::activeFindMatchRect()
{
    ASSERT(!parent());

    if (!isActiveMatchFrameValid())
        return WebFloatRect();

    return WebFloatRect(findInPageRectFromRange(m_currentActiveMatchFrame->m_activeMatch.get()));
}

void WebFrameImpl::findMatchRects(WebVector<WebFloatRect>& outputRects)
{
    ASSERT(!parent());

    Vector<WebFloatRect> matchRects;
    for (WebFrameImpl* frame = this; frame; frame = static_cast<WebFrameImpl*>(frame->traverseNext(false)))
        frame->appendFindMatchRects(matchRects);

    outputRects = matchRects;
}

void WebFrameImpl::appendFindMatchRects(Vector<WebFloatRect>& frameRects)
{
    updateFindMatchRects();
    frameRects.reserveCapacity(frameRects.size() + m_findMatchesCache.size());
    for (Vector<FindMatch>::const_iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it) {
        ASSERT(!it->m_rect.isEmpty());
        frameRects.append(it->m_rect);
    }
}

int WebFrameImpl::selectNearestFindMatch(const WebFloatPoint& point, WebRect* selectionRect)
{
    ASSERT(!parent());

    WebFrameImpl* bestFrame = 0;
    int indexInBestFrame = -1;
    float distanceInBestFrame = FLT_MAX;

    for (WebFrameImpl* frame = this; frame; frame = static_cast<WebFrameImpl*>(frame->traverseNext(false))) {
        float distanceInFrame;
        int indexInFrame = frame->nearestFindMatch(point, distanceInFrame);
        if (distanceInFrame < distanceInBestFrame) {
            bestFrame = frame;
            indexInBestFrame = indexInFrame;
            distanceInBestFrame = distanceInFrame;
        }
    }

    if (indexInBestFrame != -1)
        return bestFrame->selectFindMatch(static_cast<unsigned>(indexInBestFrame), selectionRect);

    return -1;
}

int WebFrameImpl::nearestFindMatch(const FloatPoint& point, float& distanceSquared)
{
    updateFindMatchRects();

    int nearest = -1;
    distanceSquared = FLT_MAX;
    for (size_t i = 0; i < m_findMatchesCache.size(); ++i) {
        ASSERT(!m_findMatchesCache[i].m_rect.isEmpty());
        FloatSize offset = point - m_findMatchesCache[i].m_rect.center();
        float width = offset.width();
        float height = offset.height();
        float currentDistanceSquared = width * width + height * height;
        if (currentDistanceSquared < distanceSquared) {
            nearest = i;
            distanceSquared = currentDistanceSquared;
        }
    }
    return nearest;
}

int WebFrameImpl::selectFindMatch(unsigned index, WebRect* selectionRect)
{
    ASSERT(index < m_findMatchesCache.size());

    RefPtr<Range> range = m_findMatchesCache[index].m_range;
    if (!range->boundaryPointsValid() || !range->startContainer()->inDocument())
        return -1;

    // Check if the match is already selected.
    WebFrameImpl* activeMatchFrame = viewImpl()->mainFrameImpl()->m_currentActiveMatchFrame;
    if (this != activeMatchFrame || !m_activeMatch || !areRangesEqual(m_activeMatch.get(), range.get())) {
        if (isActiveMatchFrameValid())
            activeMatchFrame->setMarkerActive(activeMatchFrame->m_activeMatch.get(), false);

        m_activeMatchIndexInCurrentFrame = m_findMatchesCache[index].m_ordinal - 1;

        // Set this frame as the active frame (the one with the active highlight).
        viewImpl()->mainFrameImpl()->m_currentActiveMatchFrame = this;
        viewImpl()->setFocusedFrame(this);

        m_activeMatch = range.release();
        setMarkerActive(m_activeMatch.get(), true);

        // Clear any user selection, to make sure Find Next continues on from the match we just activated.
        frame()->selection()->clear();

        // Make sure no node is focused. See http://crbug.com/38700.
        frame()->document()->setFocusedNode(0);
    }

    IntRect activeMatchRect;
    IntRect activeMatchBoundingBox = enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(m_activeMatch.get()));

    if (!activeMatchBoundingBox.isEmpty()) {
        if (m_activeMatch->firstNode() && m_activeMatch->firstNode()->renderer())
            m_activeMatch->firstNode()->renderer()->scrollRectToVisible(activeMatchBoundingBox,
                    ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);

        // Zoom to the active match.
        activeMatchRect = frameView()->contentsToWindow(activeMatchBoundingBox);
        viewImpl()->zoomToFindInPageRect(activeMatchRect);
    }

    if (selectionRect)
        *selectionRect = activeMatchRect;

    return ordinalOfFirstMatchForFrame(this) + m_activeMatchIndexInCurrentFrame + 1;
}

void WebFrameImpl::deliverIntent(const WebIntent& intent, WebMessagePortChannelArray* ports, WebDeliveredIntentClient* intentClient)
{
#if ENABLE(WEB_INTENTS)
    OwnPtr<WebCore::DeliveredIntentClient> client(adoptPtr(new DeliveredIntentClientImpl(intentClient)));

    WebSerializedScriptValue intentData = WebSerializedScriptValue::fromString(intent.data());
    const WebCore::Intent* webcoreIntent = intent;

    // See PlatformMessagePortChannel.cpp
    OwnPtr<MessagePortChannelArray> channels;
    if (ports && ports->size()) {
        channels = adoptPtr(new MessagePortChannelArray(ports->size()));
        for (size_t i = 0; i < ports->size(); ++i) {
            RefPtr<PlatformMessagePortChannel> platformChannel = PlatformMessagePortChannel::create((*ports)[i]);
            (*ports)[i]->setClient(platformChannel.get());
            (*channels)[i] = MessagePortChannel::create(platformChannel);
        }
    }
    OwnPtr<MessagePortArray> portArray = WebCore::MessagePort::entanglePorts(*(frame()->document()), channels.release());

    RefPtr<DeliveredIntent> deliveredIntent = DeliveredIntent::create(frame(), client.release(), intent.action(), intent.type(), intentData, portArray.release(), webcoreIntent->extras());

    DOMWindowIntents::from(frame()->document()->domWindow())->deliver(deliveredIntent.release());
#endif
}

WebString WebFrameImpl::contentAsText(size_t maxChars) const
{
    if (!frame())
        return WebString();
    Vector<UChar> text;
    frameContentAsPlainText(maxChars, frame(), &text);
    return String::adopt(text);
}

WebString WebFrameImpl::contentAsMarkup() const
{
    if (!frame())
        return WebString();
    return createFullMarkup(frame()->document());
}

WebString WebFrameImpl::renderTreeAsText(RenderAsTextControls toShow) const
{
    RenderAsTextBehavior behavior = RenderAsTextBehaviorNormal;

    if (toShow & RenderAsTextDebug)
        behavior |= RenderAsTextShowCompositedLayers | RenderAsTextShowAddresses | RenderAsTextShowIDAndClass | RenderAsTextShowLayerNesting;

    if (toShow & RenderAsTextPrinting)
        behavior |= RenderAsTextPrintingMode;

    return externalRepresentation(frame(), behavior);
}

WebString WebFrameImpl::markerTextForListItem(const WebElement& webElement) const
{
    return WebCore::markerTextForListItem(const_cast<Element*>(webElement.constUnwrap<Element>()));
}

void WebFrameImpl::printPagesWithBoundaries(WebCanvas* canvas, const WebSize& pageSizeInPixels)
{
    ASSERT(m_printContext);

    GraphicsContextBuilder builder(canvas);
    GraphicsContext& graphicsContext = builder.context();
    graphicsContext.platformContext()->setPrinting(true);

    m_printContext->spoolAllPagesWithBoundaries(graphicsContext, FloatSize(pageSizeInPixels.width, pageSizeInPixels.height));
}

WebRect WebFrameImpl::selectionBoundsRect() const
{
    return hasSelection() ? WebRect(IntRect(frame()->selection()->bounds(false))) : WebRect();
}

bool WebFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    if (!frame())
        return false;
    return frame()->editor()->selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

WebString WebFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    if (!frame())
        return WebString();
    
    LayerTreeFlags flags = showDebugInfo ? LayerTreeFlagsIncludeDebugInfo : 0;
    return WebString(frame()->layerTreeAsText(flags));
}

#if defined(__LB_SHELL__)
WebString WebFrameImpl::layerBackingsInfo() const {
    return WebString(frame()->layerBackingsInfo());
}
#endif

// WebFrameImpl public ---------------------------------------------------------

PassRefPtr<WebFrameImpl> WebFrameImpl::create(WebFrameClient* client)
{
    return adoptRef(new WebFrameImpl(client));
}

WebFrameImpl::WebFrameImpl(WebFrameClient* client)
    : FrameDestructionObserver(0)
    , m_frameLoaderClient(this)
    , m_client(client)
    , m_currentActiveMatchFrame(0)
    , m_activeMatchIndexInCurrentFrame(-1)
    , m_locatingActiveRect(false)
    , m_resumeScopingFromRange(0)
    , m_lastMatchCount(-1)
    , m_totalMatchCount(-1)
    , m_framesScopingCount(-1)
    , m_findRequestIdentifier(-1)
    , m_scopingInProgress(false)
    , m_lastFindRequestCompletedWithNoMatches(false)
    , m_nextInvalidateAfter(0)
    , m_findMatchMarkersVersion(0)
    , m_findMatchRectsAreValid(false)
    , m_animationController(this)
    , m_identifier(generateFrameIdentifier())
    , m_inSameDocumentHistoryLoad(false)
{
    WebKit::Platform::current()->incrementStatsCounter(webFrameActiveCount);
    frameCount++;
}

WebFrameImpl::~WebFrameImpl()
{
    WebKit::Platform::current()->decrementStatsCounter(webFrameActiveCount);
    frameCount--;

    cancelPendingScopingEffort();
}

void WebFrameImpl::setWebCoreFrame(WebCore::Frame* frame)
{
    ASSERT(frame);
    observeFrame(frame);
}

void WebFrameImpl::initializeAsMainFrame(WebCore::Page* page)
{
    RefPtr<Frame> mainFrame = Frame::create(page, 0, &m_frameLoaderClient);
    setWebCoreFrame(mainFrame.get());

    // Add reference on behalf of FrameLoader.  See comments in
    // WebFrameLoaderClient::frameLoaderDestroyed for more info.
    ref();

    // We must call init() after m_frame is assigned because it is referenced
    // during init().
    frame()->init();
}

PassRefPtr<Frame> WebFrameImpl::createChildFrame(const FrameLoadRequest& request, HTMLFrameOwnerElement* ownerElement)
{
    RefPtr<WebFrameImpl> webframe(adoptRef(new WebFrameImpl(m_client)));

    // Add an extra ref on behalf of the Frame/FrameLoader, which references the
    // WebFrame via the FrameLoaderClient interface. See the comment at the top
    // of this file for more info.
    webframe->ref();

    RefPtr<Frame> childFrame = Frame::create(frame()->page(), ownerElement, &webframe->m_frameLoaderClient);
    webframe->setWebCoreFrame(childFrame.get());

    childFrame->tree()->setName(request.frameName());

    frame()->tree()->appendChild(childFrame);

    // Frame::init() can trigger onload event in the parent frame,
    // which may detach this frame and trigger a null-pointer access
    // in FrameTree::removeChild. Move init() after appendChild call
    // so that webframe->mFrame is in the tree before triggering
    // onload event handler.
    // Because the event handler may set webframe->mFrame to null,
    // it is necessary to check the value after calling init() and
    // return without loading URL.
    // (b:791612)
    childFrame->init(); // create an empty document
    if (!childFrame->tree()->parent())
        return 0;

    frame()->loader()->loadURLIntoChildFrame(request.resourceRequest().url(), request.resourceRequest().httpReferrer(), childFrame.get());

    // A synchronous navigation (about:blank) would have already processed
    // onload, so it is possible for the frame to have already been destroyed by
    // script in the page.
    if (!childFrame->tree()->parent())
        return 0;

    if (m_client)
        m_client->didCreateFrame(this, webframe.get());

    return childFrame.release();
}

void WebFrameImpl::didChangeContentsSize(const IntSize& size)
{
    // This is only possible on the main frame.
    if (m_totalMatchCount > 0) {
        ASSERT(!parent());
        ++m_findMatchMarkersVersion;
    }
}

void WebFrameImpl::createFrameView()
{
    TRACE_EVENT0("webkit", "WebFrameImpl::createFrameView");

    ASSERT(frame()); // If frame() doesn't exist, we probably didn't init properly.

    WebViewImpl* webView = viewImpl();
    bool isMainFrame = webView->mainFrameImpl()->frame() == frame();
    if (isMainFrame)
        webView->suppressInvalidations(true);
    Color default_background(Color::white);
#if defined(__LB_SHELL__) && defined(__LB_SHELL__FOR_RELEASE__)
    // default background color is black on LB_SHELL
    default_background = Color::black;
#endif
    m_frame->createView(webView->size(), default_background, webView->isTransparent(),  webView->fixedLayoutSize(), IntRect(), isMainFrame ? webView->isFixedLayoutModeEnabled() : 0);

    if (webView->shouldAutoResize() && isMainFrame)
        frame()->view()->enableAutoSizeMode(true, webView->minAutoSize(), webView->maxAutoSize());

    if (isMainFrame)
        webView->suppressInvalidations(false);

#if ENABLE_INSPECTOR 
    if (isMainFrame && webView->devToolsAgentPrivate())
        webView->devToolsAgentPrivate()->mainFrameViewCreated(this);
#endif
}

WebFrameImpl* WebFrameImpl::fromFrame(Frame* frame)
{
    if (!frame)
        return 0;
    return static_cast<FrameLoaderClientImpl*>(frame->loader()->client())->webFrame();
}

WebFrameImpl* WebFrameImpl::fromFrameOwnerElement(Element* element)
{
    // FIXME: Why do we check specifically for <iframe> and <frame> here? Why can't we get the WebFrameImpl from an <object> element, for example.
    if (!element || !element->isFrameOwnerElement() || (!element->hasTagName(HTMLNames::iframeTag) && !element->hasTagName(HTMLNames::frameTag)))
        return 0;
    HTMLFrameOwnerElement* frameElement = static_cast<HTMLFrameOwnerElement*>(element);
    return fromFrame(frameElement->contentFrame());
}

WebViewImpl* WebFrameImpl::viewImpl() const
{
    if (!frame())
        return 0;
    return WebViewImpl::fromPage(frame()->page());
}

WebDataSourceImpl* WebFrameImpl::dataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(dataSource());
}

WebDataSourceImpl* WebFrameImpl::provisionalDataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(provisionalDataSource());
}

void WebFrameImpl::setFindEndstateFocusAndSelection()
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (this == mainFrameImpl->activeMatchFrame() && m_activeMatch.get()) {
        // If the user has set the selection since the match was found, we
        // don't focus anything.
        VisibleSelection selection(frame()->selection()->selection());
        if (!selection.isNone())
            return;

        // Try to find the first focusable node up the chain, which will, for
        // example, focus links if we have found text within the link.
        Node* node = m_activeMatch->firstNode();
        if (node && node->isInShadowTree()) {
            Node* host = node->shadowAncestorNode();
            if (host->hasTagName(HTMLNames::inputTag) || host->hasTagName(HTMLNames::textareaTag))
                node = host;
        }
        while (node && !node->isFocusable() && node != frame()->document())
            node = node->parentNode();

        if (node && node != frame()->document()) {
            // Found a focusable parent node. Set the active match as the
            // selection and focus to the focusable node.
            frame()->selection()->setSelection(m_activeMatch.get());
            frame()->document()->setFocusedNode(node);
            return;
        }

        // Iterate over all the nodes in the range until we find a focusable node.
        // This, for example, sets focus to the first link if you search for
        // text and text that is within one or more links.
        node = m_activeMatch->firstNode();
        while (node && node != m_activeMatch->pastLastNode()) {
            if (node->isFocusable()) {
                frame()->document()->setFocusedNode(node);
                return;
            }
            node = NodeTraversal::next(node);
        }

        // No node related to the active match was focusable, so set the
        // active match as the selection (so that when you end the Find session,
        // you'll have the last thing you found highlighted) and make sure that
        // we have nothing focused (otherwise you might have text selected but
        // a link focused, which is weird).
        frame()->selection()->setSelection(m_activeMatch.get());
        frame()->document()->setFocusedNode(0);

        // Finally clear the active match, for two reasons:
        // We just finished the find 'session' and we don't want future (potentially
        // unrelated) find 'sessions' operations to start at the same place.
        // The WebFrameImpl could get reused and the m_activeMatch could end up pointing
        // to a document that is no longer valid. Keeping an invalid reference around
        // is just asking for trouble.
        m_activeMatch = 0;
    }
}

void WebFrameImpl::didFail(const ResourceError& error, bool wasProvisional)
{
    if (!client())
        return;
    WebURLError webError = error;
    if (wasProvisional)
        client()->didFailProvisionalLoad(this, webError);
    else
        client()->didFailLoad(this, webError);
}

void WebFrameImpl::setCanHaveScrollbars(bool canHaveScrollbars)
{
    frame()->view()->setCanHaveScrollbars(canHaveScrollbars);
}

void WebFrameImpl::invalidateArea(AreaToInvalidate area)
{
    ASSERT(frame() && frame()->view());
    FrameView* view = frame()->view();

    if ((area & InvalidateAll) == InvalidateAll)
        view->invalidateRect(view->frameRect());
    else {
        if ((area & InvalidateContentArea) == InvalidateContentArea) {
            IntRect contentArea(
                view->x(), view->y(), view->visibleWidth(), view->visibleHeight());
            IntRect frameRect = view->frameRect();
            contentArea.move(-frameRect.x(), -frameRect.y());
            view->invalidateRect(contentArea);
        }
    }

    if ((area & InvalidateScrollbar) == InvalidateScrollbar) {
        // Invalidate the vertical scroll bar region for the view.
        Scrollbar* scrollbar = view->verticalScrollbar();
        if (scrollbar)
            scrollbar->invalidate();
    }
}

void WebFrameImpl::addMarker(Range* range, bool activeMatch)
{
    frame()->document()->markers()->addTextMatchMarker(range, activeMatch);
}

void WebFrameImpl::setMarkerActive(Range* range, bool active)
{
    WebCore::ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return;
    frame()->document()->markers()->setMarkersActive(range, active);
}

int WebFrameImpl::ordinalOfFirstMatchForFrame(WebFrameImpl* frame) const
{
    int ordinal = 0;
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    // Iterate from the main frame up to (but not including) |frame| and
    // add up the number of matches found so far.
    for (WebFrameImpl* it = mainFrameImpl; it != frame; it = static_cast<WebFrameImpl*>(it->traverseNext(true))) {
        if (it->m_lastMatchCount > 0)
            ordinal += it->m_lastMatchCount;
    }
    return ordinal;
}

bool WebFrameImpl::shouldScopeMatches(const String& searchText)
{
    // Don't scope if we can't find a frame or a view.
    // The user may have closed the tab/application, so abort.
    // Also ignore detached frames, as many find operations report to the main frame.
    if (!frame() || !frame()->view() || !frame()->page() || !hasVisibleContent())
        return false;

    ASSERT(frame()->document() && frame()->view());

    // If the frame completed the scoping operation and found 0 matches the last
    // time it was searched, then we don't have to search it again if the user is
    // just adding to the search string or sending the same search string again.
    if (m_lastFindRequestCompletedWithNoMatches && !m_lastSearchString.isEmpty()) {
        // Check to see if the search string prefixes match.
        String previousSearchPrefix =
            searchText.substring(0, m_lastSearchString.length());

        if (previousSearchPrefix == m_lastSearchString)
            return false; // Don't search this frame, it will be fruitless.
    }

    return true;
}

void WebFrameImpl::scopeStringMatchesSoon(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.append(new DeferredScopeStringMatches(this, identifier, searchText, options, reset));
}

void WebFrameImpl::callScopeStringMatches(DeferredScopeStringMatches* caller, int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.remove(m_deferredScopingWork.find(caller));
    scopeStringMatches(identifier, searchText, options, reset);

    // This needs to happen last since searchText is passed by reference.
    delete caller;
}

void WebFrameImpl::invalidateIfNecessary()
{
    if (m_lastMatchCount <= m_nextInvalidateAfter)
        return;

    // FIXME: (http://b/1088165) Optimize the drawing of the tickmarks and
    // remove this. This calculation sets a milestone for when next to
    // invalidate the scrollbar and the content area. We do this so that we
    // don't spend too much time drawing the scrollbar over and over again.
    // Basically, up until the first 500 matches there is no throttle.
    // After the first 500 matches, we set set the milestone further and
    // further out (750, 1125, 1688, 2K, 3K).
    static const int startSlowingDownAfter = 500;
    static const int slowdown = 750;

    int i = m_lastMatchCount / startSlowingDownAfter;
    m_nextInvalidateAfter += i * slowdown;
    invalidateArea(InvalidateScrollbar);
}

#if USE(V8)
void WebFrameImpl::loadJavaScriptURL(const KURL& url)
{
    // This is copied from ScriptController::executeIfJavaScriptURL.
    // Unfortunately, we cannot just use that method since it is private, and
    // it also doesn't quite behave as we require it to for bookmarklets.  The
    // key difference is that we need to suppress loading the string result
    // from evaluating the JS URL if executing the JS URL resulted in a
    // location change.  We also allow a JS URL to be loaded even if scripts on
    // the page are otherwise disabled.

    if (!frame()->document() || !frame()->page())
        return;

    RefPtr<Document> ownerDocument(frame()->document());

    // Protect privileged pages against bookmarklets and other javascript manipulations.
    if (SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(frame()->document()->url().protocol()))
        return;

    String script = decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
    ScriptValue result = frame()->script()->executeScript(script, true);

    String scriptResult;
    if (!result.getString(scriptResult))
        return;

    if (!frame()->navigationScheduler()->locationChangePending())
        frame()->document()->loader()->writer()->replaceDocument(scriptResult, ownerDocument.get());
}
#endif

void WebFrameImpl::willDetachPage()
{
    if (!frame() || !frame()->page())
        return;

    // Do not expect string scoping results from any frames that got detached
    // in the middle of the operation.
    if (m_scopingInProgress) {

        // There is a possibility that the frame being detached was the only
        // pending one. We need to make sure final replies can be sent.
        flushCurrentScopingEffort(m_findRequestIdentifier);

        cancelPendingScopingEffort();
    }
}

} // namespace WebKit
