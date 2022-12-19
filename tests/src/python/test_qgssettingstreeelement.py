# -*- coding: utf-8 -*-
"""
Test the QgsSettingsEntry classes

Run with: ctest -V -R PyQgsSettingsEntry

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""

from qgis import core as qgis_core
from qgis.core import QgsSettings, QgsSettingsTreeElement, QgsSettingsEntryString
from qgis.testing import start_app, unittest

from qgis.PyQt.QtCore import Qt
from qgis.PyQt.QtGui import QColor

__author__ = 'Denis Rouzaud'
__date__ = '19/12/2022'
__copyright__ = 'Copyright 2022, The QGIS Project'


start_app()


class TestQgsSettingsEntry(unittest.TestCase):

    def setUp(self):
        self.pluginName = "UnitTest"

    def tearDown(self):
        pass

    def test_constructor(self):
        with self.assertRaises(ValueError):
            QgsSettingsTreeElement()

        root = QgsSettings.createPluginTreeElement(self.pluginName)
        self.assertEqual(root.type(), QgsSettingsTreeElement.Type.Normal)
        pluginsElement = root.parent()
        self.assertEqual(pluginsElement.type(), QgsSettingsTreeElement.Type.Normal)
        self.assertEqual(pluginsElement.root().type(), QgsSettingsTreeElement.Type.Root)
        self.assertEqual(pluginsElement.root().parent(), None)

    def test_parent(self):
        root = QgsSettings.createPluginTreeElement(self.pluginName)
        self.assertEqual(root.type(), QgsSettingsTreeElement.Type.Normal)

        l1 = root.createChildElement(f"plugins/{self.pluginName}/level-1")
        self.assertEqual(l1.type(), QgsSettingsTreeElement.Type.Normal)
        self.assertEqual(l1.key(), "level-1")
        self.assertEqual(l1.completeKey(), f"plugins/{self.pluginName}/level-1")
        self.assertEqual(l1.parent(), root)
        self.assertEqual(root.childrenSettings(), [l1])

        l1a = l1.createChildElement("level-a")
        self.assertEqual(l1.type(), QgsSettingsTreeElement.Type.Normal)
        self.assertEqual(l1.key(), "level-a")
        self.assertEqual(l1.completeKey(), f"plugins/{self.pluginName}/level-1/level-a")
        self.assertEqual(l1a.parent(), l1)
        self.assertEqual(l1.childrenSettings(), [l1a])
        l1b = l1.createChildElement("level-b")
        self.assertEqual(l1.childrenSettings(), [l1a, l1b])

    def test_setting(self):
        root = QgsSettings.createPluginTreeElement(self.pluginName)
        setting = QgsSettingsEntryString("mysetting", root)

        self.assertEqual(setting.parent(), root)
        self.assertEqual(setting.key(), f"plugins/{self.pluginName}/mysetting")

        self.assertEqual(root.childrenSettings(), [setting])
        self.assertEqual(root.childrenElements(), [])

    def test_named_list(self):
        root = QgsSettings.createPluginTreeElement(self.pluginName)
        l1 = root.createChildElement("level-1")
        self.assertEqual(l1.namedElementsCount(), 0)
        nl = l1.createNamedListElement("my_list")
        self.assertEqual(nl.key(), "my_list")
        self.assertEqual(nl.completeKey(), f"plugins/{self.pluginName}/level-1/my_list/%1/")
        self.assertEqual(nl.namedElementsCount(), 1)
        self.assertEqual(nl.childrenElements(), [])
        self.assertEqual(nl.childrenSettings(), [])

        # nesting lists
        nl2 = nl.createNamedListElement("my_nested_list", QgsSettingsTreeElement.NamedListOption.CreateCurrentItemSetting)
        self.assertEqual(nl2.key(), "my_nested_list")
        self.assertEqual(nl2.completeKey(), f"plugins/{self.pluginName}/level-1/my_list/%1/my_nested_list/%2")
        self.assertEqual(nl2.namedElementsCount(), 2)
        self.assertEqual(nl2.childrenElements(), [])
        self.assertEqual(len(nl2.childrenSettings()), 1)  # the setting for the current selection
        self.assertEqual(nl2.childrenSettings()[0].key(), f"plugins/{self.pluginName}/level-1/my_list/%1/my_nested_list/%2/selected")

        # list with settings
        setting = QgsSettingsEntryString("mysetting", nl2)
        self.assertEqual(setting.key(), f"plugins/{self.pluginName}/level-1/my_list/%1/my_nested_list/%2/mysetting")
        self.assertEqual(setting.key(['item1', 'item2']), f"plugins/{self.pluginName}/level-1/my_list/item1/my_nested_list/item2/mysetting")
        self.assertEqual(nl2.childrenElements(), [])
        self.assertEqual(len(nl2.childrenSettings()), 2)
        self.assertEqual(nl2.childrenSettings()[1], setting)


if __name__ == '__main__':
    unittest.main()
