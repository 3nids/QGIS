# The following has been generated automatically from src/core/qgsfield.h
# monkey patching scoped based enum
#QgsField.ConfigurationFlag.None.__doc__ = "No flag is defined"
QgsField.ConfigurationFlag.NotSearchable.__doc__ = "Defines if the field is searchable (used in the locator search for instance)"
QgsField.ConfigurationFlag.HideFromWms.__doc__ = "Field is not available if layer is served as WMS from QGIS server"
QgsField.ConfigurationFlag.HideFromWfs.__doc__ = "Field is not available if layer is served as WFS from QGIS server"
#QgsField.ConfigurationFlag.__doc__ = "Configuration flags for fields\nThese flags are meant to be user-configurable\nand are not describing any information from the data provider.\n\n.. note::\n\n   Flags are expressed in the negative forms so that default flags is None.\n\n.. versionadded:: 3.16\n\n" + '* ``None``: ' + QgsField.ConfigurationFlag.None.__doc__ + '\n' + '* ``NotSearchable``: ' + QgsField.ConfigurationFlag.NotSearchable.__doc__ + '\n' + '* ``HideFromWms``: ' + QgsField.ConfigurationFlag.HideFromWms.__doc__ + '\n' + '* ``HideFromWfs``: ' + QgsField.ConfigurationFlag.HideFromWfs.__doc__
# --
QgsField.ConfigurationFlag.baseClass = QgsField
QgsField.ConfigurationFlags.baseClass = QgsField
ConfigurationFlags = QgsField  # dirty hack since SIP seems to introduce the flags in module
