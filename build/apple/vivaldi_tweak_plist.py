# Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved.

def ApplyVivaldiTweaks(plist, options):
  if (options.vivaldi_plist and options.platform == 'ios'):
    # Remove Voice Search shortcut item from Vivaldi iOS
    _UpdateUIApplicationShortcutItems(plist)
    # Update display name for Vivaldi iOS
    _UpdateiOSDisplayName(plist, options.vivaldi_release_kind)

  # Add Sparkle.
  if options.use_sparkle:
    _AddSparkleKeys(plist, options.vivaldi_release_kind)
  else:
    _RemoveSparkleKeys(plist)


# Sparkle
def _AddSparkleKeys(plist, vivaldi_release_kind):
  """Adds the Sparkle keys."""
  plist['SUScheduledCheckInterval'] = 86400
  plist['SUEnableAutomaticChecks'] = 'YES'
  plist['SUAllowsAutomaticUpdates'] = 'YES'
  # Delta update public signing key for official builds
  plist['SUPublicEDKey'] = '3tFhu3zD/nVxTY+R4nH1ISgcWa5Jk7yxLuq3aQQOIgY='
  if vivaldi_release_kind == 'vivaldi_final':
    plist['SUFeedURL'] = 'https://update.vivaldi.com/update/1.0/public/mac/appcast.xml'
  elif vivaldi_release_kind == 'vivaldi_snapshot':
    plist['SUFeedURL'] = 'https://update.vivaldi.com/update/1.0/snapshot/mac/appcast.xml'
  else: #vivaldi_sopranos
    plist['SUFeedURL'] = 'https://update.vivaldi.com/update/1.0/sopranos_new/mac/appcast.xml'
    # Delta update public signing key for dev builds (key created & stored on apple builder)
    plist['SUPublicEDKey'] = 'XF5qVXON+6rnl9Tj2Qy7wOkCwEqBe3kThbMVn+ObcHI='

def _RemoveSparkleKeys(plist):
  """Removes any set Sparkle keys."""
  _RemoveKeys(plist,
    'SUScheduledCheckInterval',
    'SUEnableAutomaticChecks',
    'SUAllowsAutomaticUpdates',
    'SUFeedURL',
    'SUPublicEDKey')

def _RemoveKeys(plist, *keys):
  """Removes a varargs of keys from the plist."""
  for key in keys:
    try:
      del plist[key]
    except KeyError:
      pass


# iOS
def _UpdateiOSDisplayName(plist, vivaldi_release_kind):
  """Update iOS display name based on release kind."""
  if vivaldi_release_kind == 'vivaldi_final':
    plist['CFBundleDisplayName'] = 'Vivaldi'
  else: # iOS only has final and snapshot type.
    plist['CFBundleDisplayName'] = 'Vivaldi Snapshot'

def _UpdateUIApplicationShortcutItems(plist):
  """Removes voice search related shortcut keys"""
  for idx, dict_item in enumerate(plist['UIApplicationShortcutItems']):
      if dict_item.get('UIApplicationShortcutItemIconSymbolName') == 'mic':
        try:
          del plist['UIApplicationShortcutItems'][idx]
        except:
          pass
  """Swap chrome icons names for vivaldi icons names"""
  for idx, dict_item in enumerate(plist['UIApplicationShortcutItems']):
      if dict_item.get('UIApplicationShortcutItemIconFile') == 'quick_action_incognito':
        try:
          plist['UIApplicationShortcutItems'][idx]['UIApplicationShortcutItemIconFile'] = 'vivaldi_private_tab'
        except:
          pass
  for idx, dict_item in enumerate(plist['UIApplicationShortcutItems']):
      if dict_item.get('UIApplicationShortcutItemIconSymbolName') == 'qrcode':
        try:
          del plist['UIApplicationShortcutItems'][idx]['UIApplicationShortcutItemIconSymbolName']
          plist['UIApplicationShortcutItems'][idx]['UIApplicationShortcutItemIconFile'] = 'vivaldi_qr_code'
        except:
          pass
  """Home Screen (Force touch) shortcuts"""
  # Shortcut item for Uninstall Survey
  uninstall_survey_shortcut_item = {
      'UIApplicationShortcutItemType': 'ShowUninstallSurvey',
      'UIApplicationShortcutItemTitle': 'IDS_IOS_APPLICATION_SHORTCUT_UNINSTALL_SURVEY_TITLE',
      'UIApplicationShortcutItemSubtitle': 'IDS_IOS_APPLICATION_SHORTCUT_UNINSTALL_SURVEY_SUBTITLE',
      'UIApplicationShortcutItemIconSymbolName': 'paperplane'
  }
  # Ensure 'UIApplicationShortcutItems' exists and is a list
  existing_shortcut_items = plist.get('UIApplicationShortcutItems', [])
  if not isinstance(existing_shortcut_items, list):
      existing_shortcut_items = []
  # Insert the Uninstall Survey shortcut item at the beginning of the list
  existing_shortcut_items.insert(0, uninstall_survey_shortcut_item)
  # Assign the updated list back to the plist
  plist['UIApplicationShortcutItems'] = existing_shortcut_items
# End iOS
