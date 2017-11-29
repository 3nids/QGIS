# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsCoordinateTransform.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = '(C) 2012 by Tim Sutton'
__date__ = '20/08/2012'
__copyright__ = 'Copyright 2012, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA

from qgis.core import (QgsRectangle,
                       QgsCoordinateReferenceSystem,
                       QgsCoordinateTransform,
                       QgsCoordinateTransformContext,
                       QgsProject)
from qgis.testing import start_app, unittest

start_app()


class TestQgsCoordinateTransform(unittest.TestCase):

    def testTransformBoundingBox(self):
        """Test that we can transform a rectangular bbox from utm56s to LonLat"""
        myExtent = QgsRectangle(242270, 6043737, 246330, 6045897)
        myGeoCrs = QgsCoordinateReferenceSystem()
        myGeoCrs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
        myUtmCrs = QgsCoordinateReferenceSystem()
        myUtmCrs.createFromId(32756, QgsCoordinateReferenceSystem.EpsgCrsId)
        myXForm = QgsCoordinateTransform(myUtmCrs, myGeoCrs, QgsProject.instance())
        myProjectedExtent = myXForm.transformBoundingBox(myExtent)
        myExpectedExtent = ('150.1509239873580270,-35.7176936443908772 : '
                            '150.1964384662953194,-35.6971885216629090')
        myExpectedValues = [150.1509239873580270, -35.7176936443908772,
                            150.1964384662953194, -35.6971885216629090]
        myMessage = ('Expected:\n%s\nGot:\n%s\n' %
                     (myExpectedExtent,
                      myProjectedExtent.toString()))

        self.assertAlmostEqual(myExpectedValues[0], myProjectedExtent.xMinimum(), msg=myMessage)
        self.assertAlmostEqual(myExpectedValues[1], myProjectedExtent.yMinimum(), msg=myMessage)
        self.assertAlmostEqual(myExpectedValues[2], myProjectedExtent.xMaximum(), msg=myMessage)
        self.assertAlmostEqual(myExpectedValues[3], myProjectedExtent.yMaximum(), msg=myMessage)

    def testContext(self):
        """
        Various tests to ensure that datum transforms are correctly set respecting context
        """
        context = QgsCoordinateTransformContext()
        context.addSourceDatumTransform(QgsCoordinateReferenceSystem('EPSG:28356'), 1)
        context.addDestinationDatumTransform(QgsCoordinateReferenceSystem('EPSG:4283'), 2)
        context.addSourceDestinationDatumTransform(QgsCoordinateReferenceSystem('EPSG:28356'),
                                                   QgsCoordinateReferenceSystem('EPSG:4283'),
                                                   3, 4)

        transform = QgsCoordinateTransform(QgsCoordinateReferenceSystem('EPSG:28354'), QgsCoordinateReferenceSystem('EPSG:28353'), context)
        # should be no datum transforms
        self.assertEqual(transform.sourceDatumTransform(), -1)
        self.assertEqual(transform.destinationDatumTransform(), -1)
        # matching source
        transform = QgsCoordinateTransform(QgsCoordinateReferenceSystem('EPSG:28356'), QgsCoordinateReferenceSystem('EPSG:28353'), context)
        self.assertEqual(transform.sourceDatumTransform(), 1)
        self.assertEqual(transform.destinationDatumTransform(), -1)
        # matching dest
        transform = QgsCoordinateTransform(QgsCoordinateReferenceSystem('EPSG:28354'),
                                           QgsCoordinateReferenceSystem('EPSG:4283'), context)
        self.assertEqual(transform.sourceDatumTransform(), -1)
        self.assertEqual(transform.destinationDatumTransform(), 2)
        # matching src/dest pair
        transform = QgsCoordinateTransform(QgsCoordinateReferenceSystem('EPSG:28356'),
                                           QgsCoordinateReferenceSystem('EPSG:4283'), context)
        self.assertEqual(transform.sourceDatumTransform(), 3)
        self.assertEqual(transform.destinationDatumTransform(), 4)

        # test manual overwriting
        transform.setSourceDatumTransform(11)
        transform.setDestinationDatumTransform(13)
        self.assertEqual(transform.sourceDatumTransform(), 11)
        self.assertEqual(transform.destinationDatumTransform(), 13)

        # test that auto datum setting occurs when updating src/dest crs
        transform.setSourceCrs(QgsCoordinateReferenceSystem('EPSG:28356'))
        self.assertEqual(transform.sourceDatumTransform(), 3)
        self.assertEqual(transform.destinationDatumTransform(), 4)
        transform.setSourceDatumTransform(11)
        transform.setDestinationDatumTransform(13)

        transform.setDestinationCrs(QgsCoordinateReferenceSystem('EPSG:4283'))
        self.assertEqual(transform.sourceDatumTransform(), 3)
        self.assertEqual(transform.destinationDatumTransform(), 4)
        transform.setSourceDatumTransform(11)
        transform.setDestinationDatumTransform(13)

        # delayed context set
        transform = QgsCoordinateTransform()
        self.assertEqual(transform.sourceDatumTransform(), -1)
        self.assertEqual(transform.destinationDatumTransform(), -1)
        transform.setSourceCrs(QgsCoordinateReferenceSystem('EPSG:28356'))
        transform.setDestinationCrs(QgsCoordinateReferenceSystem('EPSG:4283'))
        self.assertEqual(transform.sourceDatumTransform(), -1)
        self.assertEqual(transform.destinationDatumTransform(), -1)
        transform.setContext(context)
        self.assertEqual(transform.sourceDatumTransform(), 3)
        self.assertEqual(transform.destinationDatumTransform(), 4)

    def testProjectContext(self):
        """
        Test creating transform using convenience constructor which takes project reference
        """
        p = QgsProject()
        p.transformContext().addSourceDatumTransform(QgsCoordinateReferenceSystem('EPSG:28356'), 1)

        transform = QgsCoordinateTransform(QgsCoordinateReferenceSystem('EPSG:28356'), QgsCoordinateReferenceSystem('EPSG:28353'), p)
        self.assertEqual(transform.sourceDatumTransform(), 1)
        self.assertEqual(transform.destinationDatumTransform(), -1)


if __name__ == '__main__':
    unittest.main()
