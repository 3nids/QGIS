# The following has been generated automatically from src/gui/maptools/qgsmaptoolidentify.h
QgsMapToolIdentify.IdentifyMode.baseClass = QgsMapToolIdentify
QgsMapToolIdentify.LayerType.baseClass = QgsMapToolIdentify
LayerType = QgsMapToolIdentify  # dirty hack since SIP seems to introduce the flags in module
try:
    QgsMapToolIdentify.__attribute_docs__ = {'identifyProgress': 'Emitted when the identify action progresses.\n\n:param processed: number of objects processed so far\n:param total: total number of objects to process\n', 'identifyMessage': 'Emitted when the identify operation needs to show a user-facing message\n\n:param message: Message to show to the user\n', 'changedRasterResults': 'Emitted when the format of raster ``results`` is changed and need to be updated in user-facing displays.\n'}
except NameError:
    pass
try:
    QgsMapToolIdentify.__group__ = ['maptools']
except NameError:
    pass
try:
    QgsMapToolIdentify.IdentifyResult.__group__ = ['maptools']
except NameError:
    pass
