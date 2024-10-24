# The following has been generated automatically from src/core/textrenderer/qgsfontmanager.h
try:
    QgsFontManager.__attribute_docs__ = {'fontDownloaded': 'Emitted when a font has downloaded and been locally loaded.\n\nThe ``families`` list specifies the font families contained in the downloaded font.\n\nIf found, the ``licenseDetails`` string will be populated with corresponding font license details.\n\n.. seealso:: :py:func:`downloadAndInstallFont`\n\n.. seealso:: :py:func:`fontDownloadErrorOccurred`\n', 'fontDownloadErrorOccurred': 'Emitted when an error occurs during font downloading.\n\n.. seealso:: :py:func:`downloadAndInstallFont`\n\n.. seealso:: :py:func:`fontDownloaded`\n'}
except NameError:
    pass
QgsFontDownloadDetails.standardizeFamily = staticmethod(QgsFontDownloadDetails.standardizeFamily)
try:
    QgsFontDownloadDetails.__group__ = ['textrenderer']
except NameError:
    pass
try:
    QgsFontManager.__group__ = ['textrenderer']
except NameError:
    pass
