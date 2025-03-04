// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_SIMPLE_MENU_MODEL_H_
#define UI_BASE_MODELS_SIMPLE_MENU_MODEL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "ui/base/models/menu_model.h"

namespace gfx {
class Image;
}

namespace ui {

class ButtonMenuItemModel;

// A simple MenuModel implementation with an imperative API for adding menu
// items. This makes it easy to construct fixed menus. Menus populated by
// dynamic data sources may be better off implementing MenuModel directly.
// The breadth of MenuModel is not exposed through this API.
class UI_EXPORT SimpleMenuModel : public MenuModel {
 public:
  class UI_EXPORT Delegate {
   public:
    // Methods for determining the state of specific command ids.
    virtual bool IsCommandIdChecked(int command_id) const = 0;
    virtual bool IsCommandIdEnabled(int command_id) const = 0;
    virtual bool IsCommandIdVisible(int command_id) const;

    // Gets the accelerator for the specified command id. Returns true if the
    // command id has a valid accelerator, false otherwise.
    virtual bool GetAcceleratorForCommandId(
        int command_id,
        ui::Accelerator* accelerator) = 0;

    // Some command ids have labels and icons that change over time.
    virtual bool IsItemForCommandIdDynamic(int command_id) const;
    virtual string16 GetLabelForCommandId(int command_id) const;
    // Gets the icon for the item with the specified id, returning true if there
    // is an icon, false otherwise.
    virtual bool GetIconForCommandId(int command_id,
                                     gfx::Image* icon) const;

    // Notifies the delegate that the item with the specified command id was
    // visually highlighted within the menu.
    virtual void CommandIdHighlighted(int command_id);

    // Performs the action associated with the specified command id.
    virtual void ExecuteCommand(int command_id) = 0;
    // Performs the action associates with the specified command id
    // with |event_flags|.
    virtual void ExecuteCommand(int command_id, int event_flags);

    // Notifies the delegate that the menu is about to show.
    virtual void MenuWillShow(SimpleMenuModel* source);

    // Notifies the delegate that the menu has closed.
    virtual void MenuClosed(SimpleMenuModel* source);

   protected:
    virtual ~Delegate() {}
  };

  // The Delegate can be NULL, though if it is items can't be checked or
  // disabled.
  explicit SimpleMenuModel(Delegate* delegate);
  virtual ~SimpleMenuModel();

  // Methods for adding items to the model.
  void AddItem(int command_id, const string16& label);
  void AddItemWithStringId(int command_id, int string_id);
  void AddSeparator(MenuSeparatorType separator_type);
  void AddCheckItem(int command_id, const string16& label);
  void AddCheckItemWithStringId(int command_id, int string_id);
  void AddRadioItem(int command_id, const string16& label, int group_id);
  void AddRadioItemWithStringId(int command_id, int string_id, int group_id);

  // Adds a separator if the menu is empty, or the last item is not a separator.
  void AddSeparatorIfNecessary(MenuSeparatorType separator_type);

  // These three methods take pointers to various sub-models. These models
  // should be owned by the same owner of this SimpleMenuModel.
  void AddButtonItem(int command_id, ButtonMenuItemModel* model);
  void AddSubMenu(int command_id, const string16& label, MenuModel* model);
  void AddSubMenuWithStringId(int command_id, int string_id, MenuModel* model);

  // Methods for inserting items into the model.
  void InsertItemAt(int index, int command_id, const string16& label);
  void InsertItemWithStringIdAt(int index, int command_id, int string_id);
  void InsertSeparatorAt(int index, MenuSeparatorType separator_type);
  void InsertCheckItemAt(int index, int command_id, const string16& label);
  void InsertCheckItemWithStringIdAt(int index, int command_id, int string_id);
  void InsertRadioItemAt(
      int index, int command_id, const string16& label, int group_id);
  void InsertRadioItemWithStringIdAt(
      int index, int command_id, int string_id, int group_id);
  void InsertSubMenuAt(
      int index, int command_id, const string16& label, MenuModel* model);
  void InsertSubMenuWithStringIdAt(
      int index, int command_id, int string_id, MenuModel* model);

  // Sets the icon for the item at |index|.
  void SetIcon(int index, const gfx::Image& icon);

  // Clears all items. Note that it does not free MenuModel of submenu.
  void Clear();

  // Returns the index of the item that has the given |command_id|. Returns
  // -1 if not found.
  int GetIndexOfCommandId(int command_id);

  // Overridden from MenuModel:
  virtual bool HasIcons() const OVERRIDE;
  virtual int GetItemCount() const OVERRIDE;
  virtual ItemType GetTypeAt(int index) const OVERRIDE;
  virtual ui::MenuSeparatorType GetSeparatorTypeAt(int index) const OVERRIDE;
  virtual int GetCommandIdAt(int index) const OVERRIDE;
  virtual string16 GetLabelAt(int index) const OVERRIDE;
  virtual bool IsItemDynamicAt(int index) const OVERRIDE;
  virtual bool GetAcceleratorAt(int index,
                                ui::Accelerator* accelerator) const OVERRIDE;
  virtual bool IsItemCheckedAt(int index) const OVERRIDE;
  virtual int GetGroupIdAt(int index) const OVERRIDE;
  virtual bool GetIconAt(int index, gfx::Image* icon) OVERRIDE;
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(
      int index) const OVERRIDE;
  virtual bool IsEnabledAt(int index) const OVERRIDE;
  virtual bool IsVisibleAt(int index) const OVERRIDE;
  virtual void HighlightChangedTo(int index) OVERRIDE;
  virtual void ActivatedAt(int index) OVERRIDE;
  virtual void ActivatedAt(int index, int event_flags) OVERRIDE;
  virtual MenuModel* GetSubmenuModelAt(int index) const OVERRIDE;
  virtual void MenuWillShow() OVERRIDE;
  virtual void MenuClosed() OVERRIDE;
  virtual void SetMenuModelDelegate(
      ui::MenuModelDelegate* menu_model_delegate) OVERRIDE;
  virtual MenuModelDelegate* GetMenuModelDelegate() const OVERRIDE;

 protected:
  // Some variants of this model (SystemMenuModel) relies on items to be
  // inserted backwards. This is counter-intuitive for the API, so rather than
  // forcing customers to insert things backwards, we return the indices
  // backwards instead. That's what this method is for. By default, it just
  // returns what it's passed.
  virtual int FlipIndex(int index) const;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() { return delegate_; }

 private:
  struct Item;

  typedef std::vector<Item> ItemVector;

  // Caller needs to call FlipIndex() if necessary. Returns |index|.
  int ValidateItemIndex(int index) const;

  // Functions for inserting items into |items_|.
  void AppendItem(const Item& item);
  void InsertItemAtIndex(const Item& item, int index);
  void ValidateItem(const Item& item);

  // Notify the delegate that the menu is closed.
  void OnMenuClosed();

  ItemVector items_;

  Delegate* delegate_;

  MenuModelDelegate* menu_model_delegate_;

  base::WeakPtrFactory<SimpleMenuModel> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMenuModel);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_SIMPLE_MENU_MODEL_H_
