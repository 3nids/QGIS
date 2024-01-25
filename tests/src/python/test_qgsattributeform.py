"""QGIS Unit tests for the attribute form


Run with ctest -V -R PyQgsAttributeForm

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

"""
__author__ = 'Alessandro Pasotti'
__date__ = '2019-06-06'
__copyright__ = 'Copyright 2019, The QGIS Project'

from qgis.PyQt.QtCore import QVariant
from qgis.core import (
    QgsDefaultValue,
    QgsEditFormConfig,
    QgsEditorWidgetSetup,
    QgsFeature,
    QgsField,
    QgsVectorLayer,
)
from qgis.gui import (
    QgsAttributeEditorContext,
    QgsAttributeForm,
    QgsFilterLineEdit,
    QgsGui,
    QgsMapCanvas,
)
import unittest
from qgis.testing import start_app, QgisTestCase

QGISAPP = start_app()


class TestQgsAttributeForm(QgisTestCase):

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.mCanvas = QgsMapCanvas()
        QgsGui.editorWidgetRegistry().initEditors(cls.mCanvas)

    @classmethod
    def createLayerWithOnePoint(cls, field_type):
        layer = QgsVectorLayer(f"Point?field=fld:{field_type}",
                               "vl", "memory")
        pr = layer.dataProvider()
        f = QgsFeature()
        assert pr.addFeatures([f])
        assert layer.featureCount() == 1
        return layer

    @classmethod
    def createFormWithDuplicateWidget(cls, vl, field_type, widget_type):
        """Creates a form with two identical widgets for the same field"""

        config = vl.editFormConfig()
        config.setLayout(QgsEditFormConfig.TabLayout)
        element = config.tabs()[0]
        element2 = element.clone(element)
        config.addTab(element2)
        vl.setEditFormConfig(config)
        vl.setEditorWidgetSetup(0, QgsEditorWidgetSetup(widget_type, {}))
        form = QgsAttributeForm(vl, next(vl.getFeatures()))
        assert (form.editable())
        return form

    @classmethod
    def get_widgets_for_field(cls, vl):
        """Get compatible widget names"""

        return [k for k, v in QgsGui.editorWidgetRegistry().factories().items() if v.supportsField(vl, 0)]

    @classmethod
    def checkForm(cls, field_type, value):
        """Creates a vector layer and an associated form with two identical widgets for the same field and test it with NULL and after setting a value
        """

        vl = cls.createLayerWithOnePoint(field_type)
        assert (vl.startEditing())
        for widget_type in cls.get_widgets_for_field(vl):
            form = cls.createFormWithDuplicateWidget(vl, field_type, widget_type)
            vl.changeAttributeValue(1, 0, value)
            form.setFeature(next(vl.getFeatures()))

    def test_duplicated_widgets(self):
        """
        Note: this crashed two times for datetime (see GH #29937):

        - first crash with initial NULL values, because widget's clear() triggered changed()
        - second crash when setting a value, because setDateTime() triggered changed()

        There are no assertions in this test because we are looking for a crash.
        """

        field_types = {
            'integer': 123,
            'double': 123.45,
            'string': 'lorem ipsum',
            'date': '2019-01-01',
            'time': '12:12:12',
            'datetime': '2019-01-01',
            'int2': 123,
            'int4': 123,
            'int8': 123,
            'numeric': 123.45,
            'decimal': 123.45,
            'real': 123.45,
            'double precision': 123.45,
            'text': 'lorem ipsum',
            'bool': True,
            # 'binary'
        }

        for field_type, value in field_types.items():
            self.checkForm(field_type, value)

    def test_duplicated_widgets_multiedit(self):
        """
        Test multiedit with duplicated widgets
        """

        field_type = 'integer'
        vl = self.createLayerWithOnePoint(field_type)

        # add another point
        pr = vl.dataProvider()
        f = QgsFeature()
        assert pr.addFeatures([f])
        assert vl.featureCount() == 2

        assert vl.startEditing()

        assert vl.changeAttributeValue(1, 0, 123)
        assert vl.changeAttributeValue(2, 0, 456)

        widget_type = 'TextEdit'
        form = self.createFormWithDuplicateWidget(vl, field_type, widget_type)

        fids = list()
        for feature in vl.getFeatures():
            fids.append(feature.id())

        form.setMode(QgsAttributeEditorContext.MultiEditMode)
        form.setMultiEditFeatureIds(fids)

        for children in form.findChildren(QgsFilterLineEdit):
            if children.objectName() == 'fld':
                # As the values are mixed, the widget values should be empty
                assert not children.text()

        # After save the values should be unchanged
        form.save()
        featuresIterator = vl.getFeatures()
        self.assertEqual(next(featuresIterator).attribute(0), 123)
        self.assertEqual(next(featuresIterator).attribute(0), 456)

    def test_on_update(self):
        """Test live update, when:
        - dependency changed
        - expression contains a function
        This means:
        - changing age -> update number, birthday, pos / not update year
        - changing year -> update birthday and pos / not update number, age
        - update birthday -> update only pos
        """

        layer = QgsVectorLayer("Point?field=age:int&field=year:int&field=birthday:int&field=pos:int", "vl", "memory")

        # add another field numbers
        field = QgsField('numbers', QVariant.List, 'array')
        field.setEditorWidgetSetup(QgsEditorWidgetSetup('List', {}))
        layer.dataProvider().addAttributes([field])
        layer.updateFields()

        apply_on_update = True

        layer.setEditorWidgetSetup(0, QgsEditorWidgetSetup('Range', {}))
        layer.setEditorWidgetSetup(1, QgsEditorWidgetSetup('Range', {}))

        # set default value for birthday (2), it will depend on the field age and year
        layer.setDefaultValueDefinition(2, QgsDefaultValue('year - age', apply_on_update))
        layer.setEditorWidgetSetup(2, QgsEditorWidgetSetup('Range', {}))

        # set default value for pos (3), it contains a function and should update always (and it contains it's own value, to evaluate if it's changing)
        layer.setDefaultValueDefinition(3, QgsDefaultValue('pos + age + rand(0,0)', apply_on_update))
        layer.setEditorWidgetSetup(3, QgsEditorWidgetSetup('Range', {}))

        # set default value for numbers (4), it will depend on the field age
        layer.setDefaultValueDefinition(4, QgsDefaultValue('array(1, age)', apply_on_update))
        layer.setEditorWidgetSetup(4, QgsEditorWidgetSetup('List', {}))

        layer.startEditing()
        form = QgsAttributeForm(layer)
        feature = QgsFeature(layer.fields())
        feature.setAttribute('age', 15)
        feature.setAttribute('year', 2023)
        form.setFeature(feature)
        form.setMode(QgsAttributeEditorContext.AddFeatureMode)

        QGISAPP.processEvents()

        # some changes without impact
        form.changeAttribute('birthday', 1900)
        form.changeAttribute('pos', 100)
        form.changeAttribute('numbers', [12])

        # changing age
        form.changeAttribute('age', 10)

        self.assertEqual(form.currentFormFeature()['age'], 10)
        # no change
        self.assertEqual(form.currentFormFeature()['year'], 2023)
        # change because dependency
        self.assertEqual(form.currentFormFeature()['birthday'], 2013)
        # change because function and dependency: old value 100 + new value 10
        self.assertEqual(form.currentFormFeature()['pos'], 110)
        # change because dependency
        self.assertEqual(form.currentFormFeature()['numbers'], [1, 10])

        # changing year
        form.changeAttribute('year', 2024)

        self.assertEqual(form.currentFormFeature()['year'], 2024)
        # no change
        self.assertEqual(form.currentFormFeature()['age'], 10)
        # change because dependency
        self.assertEqual(form.currentFormFeature()['birthday'], 2014)
        # change because function: old value 110 + new value 10
        self.assertEqual(form.currentFormFeature()['pos'], 120)
        # no change
        self.assertEqual(form.currentFormFeature()['numbers'], [1, 10])

        # changing mode
        form.save()
        form.setMode(QgsAttributeEditorContext.SingleEditMode)

        # changing birthday
        form.changeAttribute('birthday', 2200)

        self.assertEqual(form.currentFormFeature()['birthday'], 2200)
        # no change
        self.assertEqual(form.currentFormFeature()['age'], 10)
        # change because dependency
        self.assertEqual(form.currentFormFeature()['year'], 2024)
        # change because function: old value 120 + new value 10
        self.assertEqual(form.currentFormFeature()['pos'], 130)
        # no change
        self.assertEqual(form.currentFormFeature()['numbers'], [1, 10])


if __name__ == '__main__':
    unittest.main()
