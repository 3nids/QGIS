# The following has been generated automatically from src/core/vector/qgsvectorlayereditbuffer.h
try:
    QgsVectorLayerEditBuffer.__attribute_docs__ = {'layerModified': 'Emitted when modifications has been done on layer\n', 'featureAdded': 'Emitted when a feature has been added to the buffer\n', 'featureDeleted': 'Emitted when a feature was deleted from the buffer\n', 'geometryChanged': "Emitted when a feature's geometry is changed.\n\n:param fid: feature ID\n:param geom: new feature geometry\n", 'attributeValueChanged': "Emitted when a feature's attribute value has been changed.\n", 'attributeAdded': 'Emitted when an attribute was added to the buffer.\n', 'attributeDeleted': 'Emitted when an attribute was deleted from the buffer.\n', 'attributeRenamed': 'Emitted when an attribute has been renamed\n\n:param idx: attribute index\n:param newName: new attribute name\n', 'committedAttributesDeleted': 'Emitted after attribute deletion has been committed to the layer.\n', 'committedAttributesAdded': 'Emitted after attribute addition has been committed to the layer.\n', 'committedAttributesRenamed': 'Emitted after committing an attribute rename\n\n:param layerId: ID of layer\n:param renamedAttributes: map of field index to new name\n', 'committedFeaturesAdded': 'Emitted after feature addition has been committed to the layer.\n', 'committedFeaturesRemoved': 'Emitted after feature removal has been committed to the layer.\n', 'committedAttributeValuesChanges': 'Emitted after feature attribute value changes have been committed to the layer.\n', 'committedGeometriesChanges': 'Emitted after feature geometry changes have been committed to the layer.\n'}
except NameError:
    pass
try:
    QgsVectorLayerEditBuffer.__group__ = ['vector']
except NameError:
    pass
