# The following has been generated automatically from src/core/settings/qgssettingstreeelement.h
# monkey patching scoped based enum
QgsSettingsTreeElement.Type.Root.__doc__ = ""
QgsSettingsTreeElement.Type.Normal.__doc__ = ""
QgsSettingsTreeElement.Type.NamedList.__doc__ = ""
QgsSettingsTreeElement.Type.__doc__ = '\n\n' + '* ``Root``: ' + QgsSettingsTreeElement.Type.Root.__doc__ + '\n' + '* ``Normal``: ' + QgsSettingsTreeElement.Type.Normal.__doc__ + '\n' + '* ``NamedList``: ' + QgsSettingsTreeElement.Type.NamedList.__doc__
# --
QgsSettingsTreeElement.Type.baseClass = QgsSettingsTreeElement
# monkey patching scoped based enum
QgsSettingsTreeElement.NamedListOption.CreateCurrentItemSetting.__doc__ = ""
QgsSettingsTreeElement.NamedListOption.__doc__ = 'Options for named list elements\n\n' + '* ``CreateCurrentItemSetting``: ' + QgsSettingsTreeElement.NamedListOption.CreateCurrentItemSetting.__doc__
# --
QgsSettingsTreeElement.NamedListOption.baseClass = QgsSettingsTreeElement
QgsSettingsTreeElement.NamedListOptions.baseClass = QgsSettingsTreeElement
NamedListOptions = QgsSettingsTreeElement  # dirty hack since SIP seems to introduce the flags in module
