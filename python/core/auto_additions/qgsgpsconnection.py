# The following has been generated automatically from src/core/gps/qgsgpsconnection.h
try:
    QgsGpsConnection.__attribute_docs__ = {'stateChanged': 'Emitted whenever the GPS state is changed.\n', 'nmeaSentenceReceived': 'Emitted whenever the GPS device receives a raw NMEA sentence.\n', 'fixStatusChanged': 'Emitted when the GPS device fix status is changed.\n\n.. versionadded:: 3.30\n', 'positionChanged': 'Emitted when the GPS position changes.\n\nThis signal is only emitted when the new GPS location is considered valid (see :py:func:`QgsGpsInformation.isValid()`).\n\n.. versionadded:: 3.30\n'}
except NameError:
    pass
try:
    QgsGpsConnection.__group__ = ['gps']
except NameError:
    pass
