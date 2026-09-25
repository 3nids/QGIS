"""QGIS Unit tests for the OAPIF provider.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""

__author__ = "Even Rouault"
__date__ = "2019-10-12"
__copyright__ = "Copyright 2019, Even Rouault"

import copy
import hashlib
import json
import os
import shutil
import tempfile
import unittest

from osgeo import gdal, ogr, osr
from providertestbase import ProviderTestCase
from qgis.core import (
    NULL,
    QgsApplication,
    QgsBox3d,
    QgsFeature,
    QgsFeatureRequest,
    QgsFeatureSink,
    QgsGeometry,
    QgsRectangle,
    QgsSettings,
    QgsSettingsTree,
    QgsTestUtils,
    QgsVectorLayer,
    QgsWkbTypes,
)
from qgis.PyQt.QtCore import QDateTime, QMetaType, Qt, QVariant
from qgis.PyQt.QtTest import QSignalSpy
from qgis.testing import QgisTestCase, start_app


def sanitize(endpoint, query_params):
    return QgsTestUtils.sanitizeFakeHttpEndpoint(f"{endpoint}{query_params}")


def GDAL_COMPUTE_VERSION(maj, min, rev):
    return (maj) * 1000000 + (min) * 10000 + (rev) * 100


ACCEPT_LANDING = "Accept=application/json"
ACCEPT_API = "Accept=application/vnd.oai.openapi+json;version=3.0, application/openapi+json;version=3.0, application/json"
ACCEPT_COLLECTION = "Accept=application/json"
ACCEPT_CONFORMANCE = "Accept=application/json"
ACCEPT_ITEMS = "Accept=application/geo+json, application/json"
ACCEPT_QUERYABLES = "Accept=application/schema+json"
ACCEPT_SCHEMA = "Accept=application/schema+json"

ARROW_MEDIA_TYPE = "application/vnd.apache.arrow.stream"
ACCEPT_ARROW = "Accept=" + ARROW_MEDIA_TYPE
# GDAL 3.8 is the first version to recognize geoarrow.wkb geometry columns
ARROW_USABLE = gdal.GetDriverByName("Arrow") is not None and int(
    gdal.VersionInfo("VERSION_NUM")
) >= GDAL_COMPUTE_VERSION(3, 8, 0)


def write_fake_response(endpoint, query_params, content):
    with open(sanitize(endpoint, query_params), "wb") as f:
        f.write(content)


def arrow_items_link(endpoint):
    return {
        "type": ARROW_MEDIA_TYPE,
        "rel": "items",
        "title": "Items as GeoArrow",
        "href": "http://" + endpoint + "/collections/mycollection/items?f=arrow",
    }


def arrow_stream(fields, features, srs="OGC:CRS84", domains=(), alternative_names=None):
    """Return an Arrow IPC stream, as a server would send it, with a WKB geometry column

    fields is a list of (name, OGR field type) or (name, OGR field type, domain name),
    features a list of (attributes, WKT), alternative_names a dict of field names to theirs
    """
    filename = "/vsimem/oapif_items.arrows"
    ds = gdal.GetDriverByName("Arrow").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(srs)
    # Neither the native GeoArrow encoding, which needs a known geometry type,
    # nor the default LZ4 compression
    lyr = ds.CreateLayer(
        "items",
        srs=sr,
        geom_type=ogr.wkbPoint,
        options=["GEOMETRY_ENCODING=WKB", "COMPRESSION=NONE"],
    )
    for domain in domains:
        ds.AddFieldDomain(domain)
    for name, field_type, *domain_name in fields:
        field = ogr.FieldDefn(name, field_type)
        if domain_name:
            field.SetDomainName(domain_name[0])
        if alternative_names and name in alternative_names:
            field.SetAlternativeName(alternative_names[name])
        lyr.CreateField(field)
    for attributes, wkt in features:
        f = ogr.Feature(lyr.GetLayerDefn())
        for name, value in attributes.items():
            f[name] = value
        f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
    del ds

    f = gdal.VSIFOpenL(filename, "rb")
    data = gdal.VSIFReadL(1, gdal.VSIStatL(filename).size, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink(filename)
    assert data[:4] == b"\xff\xff\xff\xff", "not an IPC stream"
    # GDAL 3.8 writes the ogc.wkb extension name, later versions geoarrow.wkb
    assert b"geoarrow.wkb" in data or b"ogc.wkb" in data
    # The fake HTTP endpoint would read everything up to the last CRLF as headers
    assert b"\r\n" not in data
    return data


def mergeDict(d1, d2):
    res = copy.deepcopy(d1)
    for k in d2:
        if k not in res:
            res[k] = d2[k]
        else:
            res[k] = mergeDict(res[k], d2[k])
    return res


def create_landing_page_api_collection(
    endpoint,
    extraparam="",
    storageCrs=None,
    crsList=None,
    bbox=[-71.123, 66.33, -65.32, 78.3],
    additionalApiResponse={},
    additionalConformance=[],
    collectionLinks=None,
    itemCount=None,
):

    questionmark_extraparam = "?" + extraparam if extraparam else ""

    def add_params(x, y):
        if x:
            return x + "&" + y
        return y

    # Landing page
    with open(
        sanitize(endpoint, "?" + add_params(extraparam, ACCEPT_LANDING)), "wb"
    ) as f:
        f.write(
            json.dumps(
                {
                    "links": [
                        {
                            "href": "http://"
                            + endpoint
                            + "/api"
                            + questionmark_extraparam,
                            "rel": "service-desc",
                        },
                        {
                            "href": "http://"
                            + endpoint
                            + "/collections"
                            + questionmark_extraparam,
                            "rel": "data",
                        },
                        {
                            "href": "http://"
                            + endpoint
                            + "/conformance"
                            + questionmark_extraparam,
                            "rel": "conformance",
                        },
                    ]
                }
            ).encode("UTF-8")
        )

    # API
    with open(
        sanitize(endpoint, "/api?" + add_params(extraparam, ACCEPT_API)), "wb"
    ) as f:
        j = mergeDict(
            additionalApiResponse,
            {
                "components": {
                    "parameters": {
                        "limit": {"schema": {"maximum": 1000, "default": 100}}
                    }
                }
            },
        )
        f.write(json.dumps(j).encode("UTF-8"))

    # conformance
    with open(
        sanitize(
            endpoint, "/conformance?" + add_params(extraparam, ACCEPT_CONFORMANCE)
        ),
        "wb",
    ) as f:
        f.write(
            json.dumps(
                {
                    "conformsTo": [
                        "http://www.opengis.net/spec/ogcapi-features-2/1.0/conf/crs"
                    ]
                    + additionalConformance
                }
            ).encode("UTF-8")
        )

    # collection
    collection = {
        "id": "mycollection",
        "title": "my title",
        "description": "my description",
        "extent": {"spatial": {"bbox": [bbox]}},
    }
    if bbox is None:
        del collection["extent"]
    if storageCrs:
        collection["storageCrs"] = storageCrs
    if crsList:
        collection["crs"] = crsList
    if collectionLinks:
        collection["links"] = collectionLinks
    if itemCount:
        collection["itemCount"] = itemCount

    with open(
        sanitize(
            endpoint,
            "/collections/mycollection?" + add_params(extraparam, ACCEPT_COLLECTION),
        ),
        "wb",
    ) as f:
        f.write(json.dumps(collection).encode("UTF-8"))

    # Options
    with open(
        sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
    ) as f:
        f.write(b"HEAD, GET")


class TestPyQgsOapifProvider(QgisTestCase, ProviderTestCase):
    @classmethod
    def setUpClass(cls):
        """Run before all tests"""
        super().setUpClass()

        start_app()

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        cls.basetestpath = tempfile.mkdtemp().replace("\\", "/")
        endpoint = cls.basetestpath + "/fake_qgis_http_endpoint"

        create_landing_page_api_collection(endpoint)

        items = {
            "type": "FeatureCollection",
            "numberMatched": 5,
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "pk": 1,
                        "cnt": 100,
                        "name": "Orange",
                        "name2": "oranGe",
                        "num_char": "1",
                        "dt": "2020-05-03 12:13:14",
                        "date": "2020-05-03",
                        "time": "12:13:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                },
                {
                    "type": "Feature",
                    "id": "feat.2",
                    "properties": {
                        "pk": 2,
                        "cnt": 200,
                        "name": "Apple",
                        "name2": "Apple",
                        "num_char": "2",
                        "dt": "2020-05-04 12:14:14",
                        "date": "2020-05-04",
                        "time": "12:14:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [-68.2, 70.8]},
                },
                {
                    "type": "Feature",
                    "id": "feat.3",
                    "properties": {
                        "pk": 4,
                        "cnt": 400,
                        "name": "Honey",
                        "name2": "Honey",
                        "num_char": "4",
                        "dt": "2021-05-04 13:13:14",
                        "date": "2021-05-04",
                        "time": "13:13:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [-65.32, 78.3]},
                },
                {
                    "type": "Feature",
                    "id": "feat.4",
                    "properties": {
                        "pk": 3,
                        "cnt": 300,
                        "name": "Pear",
                        "name2": "PEaR",
                        "num_char": "3",
                    },
                    "geometry": None,
                },
                {
                    "type": "Feature",
                    "id": "feat.5",
                    "properties": {
                        "pk": 5,
                        "cnt": -200,
                        "name": None,
                        "name2": "NuLl",
                        "num_char": "5",
                        "dt": "2020-05-04 12:13:14",
                        "date": "2020-05-02",
                        "time": "12:13:01",
                    },
                    "geometry": {"type": "Point", "coordinates": [-71.123, 78.23]},
                },
            ],
        }

        # limit 1 for getting count
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # first items
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Create test layer
        cls.vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        assert cls.vl.isValid()
        cls.source = cls.vl.dataProvider()

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        QgsSettings().clear()
        shutil.rmtree(cls.basetestpath, True)
        cls.vl = (
            None  # so as to properly close the provider and remove any temporary file
        )
        super().tearDownClass()

    def testCrs(self):
        self.assertEqual(self.source.sourceCrs().authid(), "OGC:CRS84")

    def testExtentSubsetString(self):
        # can't run the base provider test suite here - WFS/OAPIF extent handling is different
        # to other providers
        pass

    def testFeaturePaging(self):

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testFeaturePaging"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())

        # first real page
        first_page = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                },
                {
                    "type": "Feature",
                    "id": "feat.2",
                    "properties": {"pk": 2, "cnt": 200},
                    "geometry": {"type": "Point", "coordinates": [-68.2, 70.8]},
                },
            ],
            "links": [
                # Test multiple media types for next
                {
                    "href": "http://" + endpoint + "/second_page.html",
                    "rel": "next",
                    "type": "text/html",
                },
                {
                    "href": "http://" + endpoint + "/second_page",
                    "rel": "next",
                    "type": "application/geo+json",
                },
                {
                    "href": "http://" + endpoint + "/second_page.xml",
                    "rel": "next",
                    "type": "text/xml",
                },
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_page).encode("UTF-8"))

        # second page
        second_page = {
            "type": "FeatureCollection",
            "features": [
                # Also add a non expected property
                {
                    "type": "Feature",
                    "id": "feat.3",
                    "properties": {"a_non_expected": "foo", "pk": 4, "cnt": 400},
                    "geometry": {"type": "Point", "coordinates": [-65.32, 78.3]},
                }
            ],
            "links": [{"href": "http://" + endpoint + "/third_page", "rel": "next"}],
        }
        with open(sanitize(endpoint, "/second_page?" + ACCEPT_ITEMS), "wb") as f:
            f.write(json.dumps(second_page).encode("UTF-8"))

        # third page
        third_page = {
            "type": "FeatureCollection",
            "features": [],
            "links": [
                {
                    "href": "http://" + endpoint + "/third_page",
                    "rel": "next",
                }  # dummy link to ourselves
            ],
        }
        with open(sanitize(endpoint, "/third_page?" + ACCEPT_ITEMS), "wb") as f:
            f.write(json.dumps(third_page).encode("UTF-8"))

        values = [f["pk"] for f in vl.getFeatures()]
        self.assertEqual(values, [1, 2, 4])

        values = [f["pk"] for f in vl.getFeatures()]
        self.assertEqual(values, [1, 2, 4])

    def testBbox(self):

        endpoint = self.__class__.basetestpath + "/fake_qgis_http_endpoint_testBbox"
        create_landing_page_api_collection(
            endpoint, storageCrs="http://www.opengis.net/def/crs/EPSG/0/4326"
        )

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                },
                {
                    "type": "Feature",
                    "id": "feat.2",
                    "properties": {"pk": 2, "cnt": 200},
                    "geometry": {"type": "Point", "coordinates": [70.8, -68.2]},
                },
            ],
        }
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&bbox=65.5,-71,78,-65&bbox-crs=http://www.opengis.net/def/crs/EPSG/0/4326&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        extent = QgsRectangle(-71, 65.5, -65, 78)
        request = QgsFeatureRequest().setFilterRect(extent)
        values = [f["pk"] for f in vl.getFeatures(request)]
        self.assertEqual(values, [1, 2])

        # Test request inside above one
        EPS = 0.1
        extent = QgsRectangle(-71 + EPS, 65.5 + EPS, -65 - EPS, 78 - EPS)
        request = QgsFeatureRequest().setFilterRect(extent)
        values = [f["pk"] for f in vl.getFeatures(request)]
        self.assertEqual(values, [1, 2])

        # Test clamping of bbox
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&bbox=64.5,-180,78,-65&bbox-crs=http://www.opengis.net/def/crs/EPSG/0/4326&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        extent = QgsRectangle(-190, 64.5, -65, 78)
        request = QgsFeatureRequest().setFilterRect(extent)
        values = [f["pk"] for f in vl.getFeatures(request)]
        self.assertEqual(values, [1, 2])

        # Test request completely outside of -180,-90,180,90
        extent = QgsRectangle(-1000, -1000, -900, -900)
        request = QgsFeatureRequest().setFilterRect(extent)
        values = [f["pk"] for f in vl.getFeatures(request)]
        self.assertEqual(values, [])

        # Test request containing -180,-90,180,90
        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                },
                {
                    "type": "Feature",
                    "id": "feat.2",
                    "properties": {"pk": 2, "cnt": 200},
                    "geometry": {"type": "Point", "coordinates": [70.8, -68.2]},
                },
                {
                    "type": "Feature",
                    "id": "feat.3",
                    "properties": {"pk": 4, "cnt": 400},
                    "geometry": {"type": "Point", "coordinates": [78.3, -65.32]},
                },
            ],
        }
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&bbox=-90,-180,90,180&bbox-crs=http://www.opengis.net/def/crs/EPSG/0/4326&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        extent = QgsRectangle(-181, -91, 181, 91)
        request = QgsFeatureRequest().setFilterRect(extent)
        values = [f["pk"] for f in vl.getFeatures(request)]
        self.assertEqual(values, [1, 2, 4])

    def testLayerMetadata(self):

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testLayerMetadata"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        # API
        with open(sanitize(endpoint, "/api?" + ACCEPT_API), "wb") as f:
            f.write(
                json.dumps(
                    {
                        "components": {
                            "parameters": {
                                "limit": {"schema": {"maximum": 1000, "default": 100}}
                            }
                        },
                        "info": {
                            "contact": {
                                "name": "contact_name",
                                "email": "contact_email",
                                "url": "contact_url",
                            }
                        },
                    }
                ).encode("UTF-8")
            )

        # collection
        base_collection = {
            "id": "mycollection",
            "title": "my title",
            "description": "my description",
            "extent": {
                "spatial": {
                    "bbox": [
                        [-71.123, 66.33, -65.32, 78.3],
                        None,  # invalid
                        [1, 2, 3],  # invalid
                        ["invalid", 1, 2, 3],  # invalid
                        [2, 49, -100, 3, 50, 100],
                    ]
                },
                "temporal": {
                    "interval": [
                        [None, None],  # invalid
                        ["invalid", "invalid"],
                        "another_invalid",
                        ["1980-01-01T12:34:56.789Z", "2020-01-01T00:00:00Z"],
                        ["1980-01-01T12:34:56.789Z", None],
                        [None, "2020-01-01T00:00:00Z"],
                    ]
                },
            },
            "links": [
                {
                    "href": "href_self",
                    "rel": "self",
                    "type": "application/json",
                    "title": "my self link",
                },
                {"href": "href_parent", "rel": "parent", "title": "my parent link"},
                {
                    "href": "http://download.example.org/buildings.gpkg",
                    "rel": "enclosure",
                    "type": "application/geopackage+sqlite3",
                    "title": "Bulk download (GeoPackage)",
                    "length": 123456789012345,
                },
            ],
            # STAC specific
            "keywords": ["keyword_a", "keyword_b"],
        }

        collection = copy.deepcopy(base_collection)
        collection["links"].append(
            {
                "href": "https://creativecommons.org/publicdomain/zero/1.0/",
                "rel": "license",
                "type": "text/html",
                "title": "CC0-1.0",
            }
        )
        collection["links"].append(
            {
                "href": "https://creativecommons.org/publicdomain/zero/1.0/rdf",
                "rel": "license",
                "type": "application/rdf+xml",
                "title": "CC0-1.0",
            }
        )
        collection["links"].append(
            {
                "href": "https://example.com",
                "rel": "license",
                "type": "text/html",
                "title": "Public domain",
            }
        )
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert md.identifier() == "href_self"
        assert md.parentIdentifier() == "href_parent"
        assert md.type() == "dataset"
        assert md.title() == "my title"
        assert md.abstract() == "my description"

        contacts = md.contacts()
        assert len(contacts) == 1
        contact = contacts[0]
        assert contact.name == "contact_name"
        assert contact.email == "contact_email"
        assert contact.organization == "contact_url"

        assert len(md.licenses()) == 2
        assert md.licenses()[0] == "CC0-1.0"
        assert md.licenses()[1] == "Public domain"

        assert "keywords" in md.keywords()
        assert md.keywords()["keywords"] == ["keyword_a", "keyword_b"]

        assert md.crs().isValid()
        assert md.crs().authid() == "OGC:CRS84"
        assert md.crs().isGeographic()
        assert not md.crs().hasAxisInverted()

        links = md.links()
        assert len(links) == 6, len(links)
        assert links[0].type == "WWW:LINK"
        assert links[0].url == "href_self"
        assert links[0].name == "self"
        assert links[0].mimeType == "application/json"
        assert links[0].description == "my self link"
        assert links[0].size == ""
        assert links[2].size == "123456789012345"

        extent = md.extent()
        assert len(extent.spatialExtents()) == 2
        spatialExtent = extent.spatialExtents()[0]
        assert spatialExtent.extentCrs.isValid()
        assert spatialExtent.extentCrs.isGeographic()
        assert not spatialExtent.extentCrs.hasAxisInverted()
        assert spatialExtent.bounds == QgsBox3d(
            -71.123, 66.33, float("nan"), -65.32, 78.3, float("nan")
        )
        spatialExtent = extent.spatialExtents()[1]
        assert spatialExtent.bounds == QgsBox3d(2, 49, -100, 3, 50, 100)

        temporalExtents = extent.temporalExtents()
        assert len(temporalExtents) == 3
        assert temporalExtents[0].begin() == QDateTime.fromString(
            "1980-01-01T12:34:56.789Z", Qt.DateFormat.ISODateWithMs
        ), temporalExtents[0].begin()
        assert temporalExtents[0].end() == QDateTime.fromString(
            "2020-01-01T00:00:00Z", Qt.DateFormat.ISODateWithMs
        ), temporalExtents[0].end()
        assert temporalExtents[1].begin().isValid()
        assert not temporalExtents[1].end().isValid()
        assert not temporalExtents[2].begin().isValid()
        assert temporalExtents[2].end().isValid()

        # Variant using STAC license
        collection = copy.deepcopy(base_collection)
        collection["license"] = "STAC license"
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert len(md.licenses()) == 1
        assert md.licenses()[0] == "STAC license"

        # Variant using STAC license=various
        collection = copy.deepcopy(base_collection)
        collection["license"] = "various"
        collection["links"].append(
            {
                "href": "https://creativecommons.org/publicdomain/zero/1.0/",
                "rel": "license",
                "type": "text/html",
                "title": "CC0-1.0",
            }
        )
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert len(md.licenses()) == 1
        assert md.licenses()[0] == "CC0-1.0"

        # Variant using STAC license=proprietary
        collection = copy.deepcopy(base_collection)
        collection["license"] = "proprietary"
        collection["links"].append(
            {
                "href": "https://example.com",
                "rel": "license",
                "type": "text/html",
                "title": "my proprietary license",
            }
        )
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert len(md.licenses()) == 1
        assert md.licenses()[0] == "my proprietary license"

        # Variant using STAC license=proprietary (non conformant: missing a rel=license link)
        collection = copy.deepcopy(base_collection)
        collection["license"] = "proprietary"
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert len(md.licenses()) == 1
        assert md.licenses()[0] == "proprietary"

        # Variant with storageCrs
        collection = copy.deepcopy(base_collection)
        collection["storageCrs"] = "http://www.opengis.net/def/crs/EPSG/0/4258"
        collection["storageCrsCoordinateEpoch"] = 2020.0
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4258&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert vl.sourceCrs().isValid()
        assert vl.sourceCrs().authid() == "EPSG:4258"
        assert vl.sourceCrs().isGeographic()
        assert vl.sourceCrs().coordinateEpoch() == 2020.0
        assert vl.sourceCrs().hasAxisInverted()

        # Variant with a list of crs
        collection = copy.deepcopy(base_collection)
        collection["crs"] = [
            "http://www.opengis.net/def/crs/EPSG/0/4258",
            "http://www.opengis.net/def/crs/EPSG/0/4326",
        ]
        with open(
            sanitize(endpoint, "/collections/mycollection?" + ACCEPT_COLLECTION), "wb"
        ) as f:
            f.write(json.dumps(collection).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        md = vl.metadata()
        assert vl.sourceCrs().isValid()
        assert vl.sourceCrs().authid() == "EPSG:4258"
        assert vl.sourceCrs().isGeographic()
        assert vl.sourceCrs().hasAxisInverted()

    def testQuotedString(self):

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testQuotedString"
        )
        create_landing_page_api_collection(endpoint)

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"foo": 'bar"baz'},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }

        no_items = {"type": "FeatureCollection", "features": []}

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["foo"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ['bar"baz'])

    def testDateTimeFiltering(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testDateTimeFiltering"
        )
        create_landing_page_api_collection(endpoint)

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"my_dt_field": "2019-10-15T00:34:00Z", "foo": "bar"},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }

        no_items = {"type": "FeatureCollection", "features": []}

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' filter='\"my_dt_field\" >= \\'2019-05-15T00:00:00Z\\''",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=2019-05-15T00:00:00Z/9999-12-31T00:00:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ["feat.1"])

        assert vl.setSubsetString(""""my_dt_field" < '2019-01-01T00:34:00Z'""")

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=0000-01-01T00:00:00Z/2019-01-01T00:34:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(no_items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, [])

        assert vl.setSubsetString(""""my_dt_field" = '2019-10-15T00:34:00Z'""")

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=2019-10-15T00:34:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ["feat.1"])

        assert vl.setSubsetString(
            """("my_dt_field" >= '2019-01-01T00:34:00Z') AND ("my_dt_field" <= '2019-12-31T00:00:00Z')"""
        )

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=2019-01-01T00:34:00Z/2019-12-31T00:00:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ["feat.1"])

        # Partial on client side
        assert vl.setSubsetString(
            """("my_dt_field" >= '2019-01-01T00:34:00Z') AND ("foo" = 'bar')"""
        )

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=2019-01-01T00:34:00Z/9999-12-31T00:00:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ["feat.1"])

        # Same but with non-matching client-side part
        assert vl.setSubsetString(
            """("my_dt_field" >= '2019-01-01T00:34:00Z') AND ("foo" != 'bar')"""
        )

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=2019-01-01T00:34:00Z/9999-12-31T00:00:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, [])

        # Switch order
        assert vl.setSubsetString(
            """("foo" = 'bar') AND ("my_dt_field" >= '2019-01-01T00:34:00Z')"""
        )

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&datetime=2019-01-01T00:34:00Z/9999-12-31T00:00:00Z&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ["feat.1"])

    def testSimpleQueryableFiltering(self):
        """Test simple filtering capabilities, not requiring Part 3"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_encoded_query_testSimpleQueryableFiltering"
        )
        additionalApiResponse = {
            "paths": {
                "/collections/mycollection/items": {
                    "get": {
                        "parameters": [
                            {
                                "$ref": "#/components/parameters/mycollection_strfield_param"
                            },
                            {
                                "name": "intfield",
                                "in": "query",
                                "style": "form",
                                "explode": False,
                                "schema": {"type": "integer"},
                            },
                            {
                                "name": "doublefield",
                                "in": "query",
                                "style": "form",
                                "explode": False,
                                "schema": {"type": "number"},
                            },
                            {
                                "name": "boolfield",
                                "in": "query",
                                "style": "form",
                                "explode": False,
                                "schema": {"type": "boolean"},
                            },
                        ]
                    }
                }
            },
            "components": {
                "parameters": {
                    "mycollection_strfield_param": {
                        "name": "strfield",
                        "in": "query",
                        "style": "form",
                        "explode": False,
                        "schema": {"type": "string"},
                    }
                }
            },
        }
        create_landing_page_api_collection(
            endpoint, additionalApiResponse=additionalApiResponse
        )

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "strfield": "foo=bar",
                        "intfield": 1,
                        "doublefield": 1.5,
                        "boolfield": True,
                    },
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }

        no_items = {"type": "FeatureCollection", "features": []}

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        assert vl.setSubsetString(
            """"strfield" = 'foo=bar' and intfield = 1 and doublefield = 1.5 and boolfield = true"""
        )

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&strfield=foo%3Dbar&intfield=1&doublefield=1.5&boolfield=true&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, ["feat.1"])

        assert vl.setSubsetString(""""strfield" = 'bar'""")

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=1000&strfield=bar&" + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(no_items).encode("UTF-8"))
        values = [f["id"] for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(values, [])

    def testCQL2TextFiltering(self):
        """Test Part 3 CQL2-Text filtering"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_encoded_query_testCQL2TextFiltering"
        )
        additionalConformance = [
            "http://www.opengis.net/spec/cql2/1.0/conf/advanced-comparison-operators",
            "http://www.opengis.net/spec/cql2/1.0/conf/basic-cql2",
            "http://www.opengis.net/spec/cql2/1.0/conf/basic-spatial-functions",
            "http://www.opengis.net/spec/cql2/1.0/conf/case-insensitive-comparison",
            "http://www.opengis.net/spec/cql2/1.0/conf/cql2-text",
            "http://www.opengis.net/spec/ogcapi-features-3/1.0/conf/features-filter",
            "http://www.opengis.net/spec/ogcapi-features-3/1.0/conf/filter",
        ]

        create_landing_page_api_collection(
            endpoint, additionalConformance=additionalConformance
        )

        filename = sanitize(
            endpoint, "/collections/mycollection/queryables?" + ACCEPT_QUERYABLES
        )
        queryables = {
            "properties": {
                "strfield": {"type": "string"},
                "strfield2": {"type": "string"},
                "intfield": {"type": "integer"},
                "doublefield": {"type": "number"},
                "boolfield": {"type": "boolean"},
                "boolfield2": {"type": "boolean"},
                "datetimefield": {
                    "type": "string",
                    "format": "date-time",
                },
                "datefield": {
                    "type": "string",
                    "format": "date",
                },
                "geometry": {"$ref": "https://geojson.org/schema/Point.json"},
            }
        }
        with open(filename, "wb") as f:
            f.write(json.dumps(queryables).encode("UTF-8"))

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "strfield": "foo=bar",
                        "strfield2": None,
                        "intfield": 1,
                        "doublefield": 1.5,
                        "not_a_queryable": 3,
                        "datetimefield": "2023-04-19T12:34:56Z",
                        "datefield": "2023-04-19",
                        "boolfield": True,
                        "boolfield2": False,
                    },
                    "geometry": {"type": "Point", "coordinates": [-70.5, 66.5]},
                }
            ],
        }

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        tests = [
            (
                """"strfield" = 'foo=bar' and intfield = 1""",
                """filter=((strfield%20%3D%20'foo%3Dbar')%20AND%20(intfield%20%3D%201))&filter-lang=cql2-text""",
            ),
            (
                """doublefield = 1.5 or boolfield = true""",
                """filter=((doublefield%20%3D%201.5)%20OR%20(boolfield%20%3D%20TRUE))&filter-lang=cql2-text""",
            ),
            (
                "boolfield2 = false",
                "filter=(boolfield2%20%3D%20FALSE)&filter-lang=cql2-text",
            ),
            (
                "NOT(intfield = 0)",
                """filter=(NOT%20((intfield%20%3D%200)))&filter-lang=cql2-text""",
            ),
            (
                "intfield <> 0",
                """filter=(intfield%20%3C%3E%200)&filter-lang=cql2-text""",
            ),
            ("intfield > 0", """filter=(intfield%20%3E%200)&filter-lang=cql2-text"""),
            (
                "intfield >= 1",
                """filter=(intfield%20%3E%3D%201)&filter-lang=cql2-text""",
            ),
            ("intfield < 2", """filter=(intfield%20%3C%202)&filter-lang=cql2-text"""),
            (
                "intfield <= 1",
                """filter=(intfield%20%3C%3D%201)&filter-lang=cql2-text""",
            ),
            (
                "intfield IN (1, 2)",
                """filter=intfield%20IN%20(1,2)&filter-lang=cql2-text""",
            ),
            (
                "intfield NOT IN (3, 4)",
                """filter=intfield%20NOT%20IN%20(3,4)&filter-lang=cql2-text""",
            ),
            (
                "intfield BETWEEN 0 AND 2",
                "filter=intfield%20BETWEEN%200%20AND%202&filter-lang=cql2-text",
            ),
            (
                "intfield NOT BETWEEN 3 AND 4",
                "filter=intfield%20NOT%20BETWEEN%203%20AND%204&filter-lang=cql2-text",
            ),
            (
                "strfield2 IS NULL",
                """filter=(strfield2%20IS%20NULL)&filter-lang=cql2-text""",
            ),
            (
                "intfield IS NOT NULL",
                """filter=(intfield%20IS%20NOT%20NULL)&filter-lang=cql2-text""",
            ),
            (
                "datetimefield = make_datetime(2023, 4, 19, 12, 34, 56)",
                "filter=(datetimefield%20%3D%20TIMESTAMP('2023-04-19T12:34:56.000Z'))&filter-lang=cql2-text",
            ),
            (
                "datetimefield = '2023-04-19T12:34:56.000Z'",
                "filter=(datetimefield%20%3D%20TIMESTAMP('2023-04-19T12:34:56.000Z'))&filter-lang=cql2-text",
            ),
            (
                "datefield = make_date(2023, 4, 19)",
                "filter=(datefield%20%3D%20DATE('2023-04-19'))&filter-lang=cql2-text",
            ),
            (
                "datefield = '2023-04-19'",
                "filter=(datefield%20%3D%20DATE('2023-04-19'))&filter-lang=cql2-text",
            ),
            (
                """"strfield" LIKE 'foo%'""",
                """filter=(strfield%20LIKE%20'foo%25')&filter-lang=cql2-text""",
            ),
            (
                """"strfield" NOT LIKE 'bar'""",
                """filter=(strfield%20NOT%20LIKE%20'bar')&filter-lang=cql2-text""",
            ),
            (
                """"strfield" ILIKE 'fo%'""",
                """filter=(CASEI(strfield)%20LIKE%20CASEI('fo%25'))&filter-lang=cql2-text""",
            ),
            (
                """"strfield" NOT ILIKE 'bar'""",
                """filter=(CASEI(strfield)%20NOT%20LIKE%20CASEI('bar'))&filter-lang=cql2-text""",
            ),
            (
                """intersects_bbox($geometry, geomFromWkt('POLYGON((-180 -90,-180 90,180 90,180 -90,-180 -90))'))""",
                """filter=S_INTERSECTS(geometry,BBOX(-180,-90,180,90))&filter-lang=cql2-text""",
            ),
            (
                """intersects($geometry, geomFromWkt('POINT(-70.5 66.5))'))""",
                """filter=S_INTERSECTS(geometry,POINT(-70.5%2066.5))&filter-lang=cql2-text""",
            ),
            # Partially evaluated on server
            (
                "intfield >= 1 AND not_a_queryable = 3",
                """filter=(intfield%20%3E%3D%201)&filter-lang=cql2-text""",
            ),
            (
                "not_a_queryable = 3 AND intfield >= 1",
                """filter=(intfield%20%3E%3D%201)&filter-lang=cql2-text""",
            ),
            # Only evaluated on client
            ("intfield >= 1 OR not_a_queryable = 3", ""),
            ("not_a_queryable = 3 AND not_a_queryable = 3", ""),
        ]
        for expr, cql_filter in tests:
            assert vl.setSubsetString(expr)

            filename = sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&"
                + cql_filter
                + ("&" if cql_filter else "")
                + ACCEPT_ITEMS,
            )
            with open(filename, "wb") as f:
                f.write(json.dumps(items).encode("UTF-8"))
            values = [f["id"] for f in vl.getFeatures()]
            os.unlink(filename)
            self.assertEqual(values, ["feat.1"], expr)

    def testCQL2TextFilteringAndPart2(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_encoded_query_testCQL2TextFilteringAndPart2"
        )
        additionalConformance = [
            "http://www.opengis.net/spec/cql2/1.0/conf/basic-cql2",
            "http://www.opengis.net/spec/cql2/1.0/conf/basic-spatial-operators",
            "http://www.opengis.net/spec/cql2/1.0/conf/cql2-text",
            "http://www.opengis.net/spec/ogcapi-features-3/1.0/conf/features-filter",
            "http://www.opengis.net/spec/ogcapi-features-3/1.0/conf/filter",
        ]

        create_landing_page_api_collection(
            endpoint,
            storageCrs="http://www.opengis.net/def/crs/EPSG/0/4258",
            crsList=[
                "http://www.opengis.net/def/crs/OGC/0/CRS84",
                "http://www.opengis.net/def/crs/EPSG/0/4258",
            ],
            additionalConformance=additionalConformance,
        )

        filename = sanitize(
            endpoint, "/collections/mycollection/queryables?" + ACCEPT_QUERYABLES
        )
        queryables = {
            "properties": {
                "geometry": {"$ref": "https://geojson.org/schema/Point.json"},
            }
        }
        with open(filename, "wb") as f:
            f.write(json.dumps(queryables).encode("UTF-8"))

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {},
                    # lat, long order
                    "geometry": {"type": "Point", "coordinates": [49, 2]},
                }
            ],
        }

        filename = sanitize(
            endpoint,
            "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4258&"
            + ACCEPT_ITEMS,
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        tests = [
            (
                """intersects($geometry, geomFromWkt('POINT(2 49))'))""",
                """filter=S_INTERSECTS(geometry,POINT(49%202))&filter-lang=cql2-text&filter-crs=http://www.opengis.net/def/crs/EPSG/0/4258""",
            ),
        ]
        for expr, cql_filter in tests:
            assert vl.setSubsetString(expr)

            filename = sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&"
                + cql_filter
                + ("&" if cql_filter else "")
                + "crs=http://www.opengis.net/def/crs/EPSG/0/4258&"
                + ACCEPT_ITEMS,
            )
            with open(filename, "wb") as f:
                f.write(json.dumps(items).encode("UTF-8"))
            values = [f["id"] for f in vl.getFeatures()]
            os.unlink(filename)
            self.assertEqual(values, ["feat.1"], expr)

    def testCQL2TextFilteringGetFeaturesExpression(self):
        """Test Part 3 CQL2-Text filtering"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_encoded_query_testCQL2TextFilteringGetFeaturesExpression"
        )
        additionalConformance = [
            "http://www.opengis.net/spec/cql2/1.0/conf/basic-cql2",
            "http://www.opengis.net/spec/cql2/1.0/conf/cql2-text",
            "http://www.opengis.net/spec/ogcapi-features-3/1.0/conf/features-filter",
            "http://www.opengis.net/spec/ogcapi-features-3/1.0/conf/filter",
        ]

        create_landing_page_api_collection(
            endpoint, additionalConformance=additionalConformance
        )

        filename = sanitize(
            endpoint, "/collections/mycollection/queryables?" + ACCEPT_QUERYABLES
        )
        queryables = {
            "properties": {
                "strfield": {"type": "string"},
                "geometry": {"$ref": "https://geojson.org/schema/Point.json"},
            }
        }
        with open(filename, "wb") as f:
            f.write(json.dumps(queryables).encode("UTF-8"))

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "strfield": "foo=bar",
                    },
                    "geometry": {"type": "Point", "coordinates": [-70.5, 66.5]},
                }
            ],
        }

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        tests = [
            (
                "",
                """"strfield" = 'foo=bar'""",
                """filter=(strfield%20%3D%20'foo%3Dbar')&filter-lang=cql2-text""",
            ),
            (
                "strfield <> 'x'",
                """"strfield" = 'foo=bar'""",
                """filter=((strfield%20%3C%3E%20'x'))%20AND%20((strfield%20%3D%20'foo%3Dbar'))&filter-lang=cql2-text""",
            ),
        ]
        for substring_expr, getfeatures_expr, cql_filter in tests:
            assert vl.setSubsetString(substring_expr)
            filename = sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&"
                + cql_filter
                + ("&" if cql_filter else "")
                + ACCEPT_ITEMS,
            )
            with open(filename, "wb") as f:
                f.write(json.dumps(items).encode("UTF-8"))
            request = QgsFeatureRequest()
            if getfeatures_expr:
                request.setFilterExpression(getfeatures_expr)
            values = [f["id"] for f in vl.getFeatures(request)]
            os.unlink(filename)
            self.assertEqual(values, ["feat.1"], (substring_expr, getfeatures_expr))

    def testStringList(self):

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testStringList"
        )
        create_landing_page_api_collection(endpoint)

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"my_stringlist_field": ["a", "b"]},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())
        os.unlink(filename)

        filename = sanitize(
            endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
        )
        with open(filename, "wb") as f:
            f.write(json.dumps(items).encode("UTF-8"))
        features = [f for f in vl.getFeatures()]
        os.unlink(filename)
        self.assertEqual(features[0]["my_stringlist_field"], ["a", "b"])

    def testApikey(self):

        endpoint = self.__class__.basetestpath + "/fake_qgis_http_endpoint_apikey"
        create_landing_page_api_collection(endpoint, extraparam="apikey=mykey")

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&apikey=mykey&" + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&apikey=mykey&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        app_log = QgsApplication.messageLog()

        # signals should be emitted by application log
        app_spy = QSignalSpy(app_log.messageReceived)

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "?apikey=mykey' typename='mycollection'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        values = [f["id"] for f in vl.getFeatures()]
        self.assertEqual(values, ["feat.1"])
        self.assertEqual(len(app_spy), 0, list(app_spy))

    def testDefaultCRS(self):

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        basetestpath = tempfile.mkdtemp().replace("\\", "/")
        endpoint = basetestpath + "/fake_qgis_http_endpoint_ogc84"

        create_landing_page_api_collection(
            endpoint, bbox=[66.33, -71.123, 78.3, -65.32]
        )

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "pk": 1,
                        "cnt": 100,
                        "name": "Orange",
                        "name2": "oranGe",
                        "num_char": "1",
                        "dt": "2020-05-03 12:13:14",
                        "date": "2020-05-03",
                        "time": "12:13:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                },
                {
                    "type": "Feature",
                    "id": "feat.2",
                    "properties": {
                        "pk": 2,
                        "cnt": 200,
                        "name": "Apple",
                        "name2": "Apple",
                        "num_char": "2",
                        "dt": "2020-05-04 12:14:14",
                        "date": "2020-05-04",
                        "time": "12:14:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [70.8, -68.2]},
                },
                {
                    "type": "Feature",
                    "id": "feat.3",
                    "properties": {
                        "pk": 4,
                        "cnt": 400,
                        "name": "Honey",
                        "name2": "Honey",
                        "num_char": "4",
                        "dt": "2021-05-04 13:13:14",
                        "date": "2021-05-04",
                        "time": "13:13:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [78.3, -65.32]},
                },
                {
                    "type": "Feature",
                    "id": "feat.4",
                    "properties": {
                        "pk": 3,
                        "cnt": 300,
                        "name": "Pear",
                        "name2": "PEaR",
                        "num_char": "3",
                    },
                    "geometry": None,
                },
                {
                    "type": "Feature",
                    "id": "feat.5",
                    "properties": {
                        "pk": 5,
                        "cnt": -200,
                        "name": None,
                        "name2": "NuLl",
                        "num_char": "5",
                        "dt": "2020-05-04 12:13:14",
                        "date": "2020-05-02",
                        "time": "12:13:01",
                    },
                    "geometry": {"type": "Point", "coordinates": [78.23, -71.123]},
                },
            ],
        }

        # first items
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Create test layer
        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        assert vl.isValid()
        source = vl.dataProvider()

        self.assertEqual(source.sourceCrs().authid(), "OGC:CRS84")

    def testCRS2056(self):

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        basetestpath = tempfile.mkdtemp().replace("\\", "/")
        endpoint = basetestpath + "/fake_qgis_http_endpoint_epsg_2056"

        create_landing_page_api_collection(
            endpoint,
            storageCrs="http://www.opengis.net/def/crs/EPSG/0/2056",
            crsList=[
                "http://www.opengis.net/def/crs/OGC/0/CRS84",
                "http://www.opengis.net/def/crs/EPSG/0/2056",
            ],
        )

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "pk": 1,
                        "cnt": 100,
                        "name": "Orange",
                        "name2": "oranGe",
                        "num_char": "1",
                        "dt": "2020-05-03 12:13:14",
                        "date": "2020-05-03",
                        "time": "12:13:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [2510100, 1155050]},
                },
                {
                    "type": "Feature",
                    "id": "feat.2",
                    "properties": {
                        "pk": 2,
                        "cnt": 200,
                        "name": "Apple",
                        "name2": "Apple",
                        "num_char": "2",
                        "dt": "2020-05-04 12:14:14",
                        "date": "2020-05-04",
                        "time": "12:14:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [2511250, 1154600]},
                },
                {
                    "type": "Feature",
                    "id": "feat.3",
                    "properties": {
                        "pk": 4,
                        "cnt": 400,
                        "name": "Honey",
                        "name2": "Honey",
                        "num_char": "4",
                        "dt": "2021-05-04 13:13:14",
                        "date": "2021-05-04",
                        "time": "13:13:14",
                    },
                    "geometry": {"type": "Point", "coordinates": [2511260, 1154610]},
                },
                {
                    "type": "Feature",
                    "id": "feat.4",
                    "properties": {
                        "pk": 3,
                        "cnt": 300,
                        "name": "Pear",
                        "name2": "PEaR",
                        "num_char": "3",
                    },
                    "geometry": None,
                },
                {
                    "type": "Feature",
                    "id": "feat.5",
                    "properties": {
                        "pk": 5,
                        "cnt": -200,
                        "name": None,
                        "name2": "NuLl",
                        "num_char": "5",
                        "dt": "2020-05-04 12:13:14",
                        "date": "2020-05-02",
                        "time": "12:13:01",
                    },
                    "geometry": {"type": "Point", "coordinates": [2511270, 1154620]},
                },
            ],
        }

        # first items
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/2056&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Create test layer
        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        assert vl.isValid()
        source = vl.dataProvider()

        self.assertEqual(source.sourceCrs().authid(), "EPSG:2056")

        # first items
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Test srsname parameter overrides default CRS
        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection' srsname='OGC:CRS84'",
            "test",
            "OAPIF",
        )
        assert vl.isValid()
        source = vl.dataProvider()

        self.assertEqual(source.sourceCrs().authid(), "OGC:CRS84")

    def testFeatureCountFallbackAndNoBboxInCollection(self):

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        basetestpath = tempfile.mkdtemp().replace("\\", "/")
        endpoint = basetestpath + "/fake_qgis_http_endpoint_feature_count_fallback"

        create_landing_page_api_collection(
            endpoint, storageCrs="http://www.opengis.net/def/crs/EPSG/0/2056", bbox=None
        )

        items = {
            "type": "FeatureCollection",
            "features": [],
            "links": [
                # should not be hit
                {"href": "http://" + endpoint + "/next_page", "rel": "next"}
            ],
        }
        for i in range(10):
            items["features"].append(
                {
                    "type": "Feature",
                    "id": f"feat.{i}",
                    "properties": {},
                    "geometry": {"type": "Point", "coordinates": [23, 63]},
                }
            )

        # first items
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # first items
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/2056&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # real page

        items = {
            "type": "FeatureCollection",
            "features": [],
            "links": [
                # should not be hit
                {"href": "http://" + endpoint + "/next_page", "rel": "next"}
            ],
        }
        for i in range(1001):
            items["features"].append(
                {
                    "type": "Feature",
                    "id": f"feat.{i}",
                    "properties": {},
                    "geometry": None,
                }
            )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/2056&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Create test layer

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        assert vl.isValid()
        source = vl.dataProvider()

        # Extent got from first fetched features
        reference = QgsGeometry.fromRect(
            QgsRectangle(3415684, 3094884, 3415684, 3094884)
        )
        vl_extent = QgsGeometry.fromRect(vl.extent())
        assert QgsGeometry.compare(
            vl_extent.asPolygon()[0], reference.asPolygon()[0], 10
        ), f"Expected {reference.asWkt()}, got {vl_extent.asWkt()}"

        app_log = QgsApplication.messageLog()
        # signals should be emitted by application log
        app_spy = QSignalSpy(app_log.messageReceived)

        self.assertEqual(source.featureCount(), 1000)

        self.assertEqual(len(app_spy), 0, list(app_spy))

    def testFeatureCountFromLDProxyItemCount(self):

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        basetestpath = tempfile.mkdtemp().replace("\\", "/")
        endpoint = basetestpath + "/fake_qgis_http_endpoint_feature_count_ldproxy"

        create_landing_page_api_collection(
            endpoint, itemCount=123456789012345, bbox=None
        )

        items = {
            "type": "FeatureCollection",
            "features": [],
        }

        # first items
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # first items
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Create test layer

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        assert vl.isValid()
        source = vl.dataProvider()

        self.assertEqual(source.featureCount(), 123456789012345)

    def testZGeometries(self):

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        basetestpath = tempfile.mkdtemp().replace("\\", "/")
        endpoint = basetestpath + "/fake_qgis_http_endpoint_testZGeometries"

        create_landing_page_api_collection(
            endpoint,
            storageCrs="http://www.opengis.net/def/crs/EPSG/0/4979",
            bbox=[1, 48, 0, 3, 50, 200],
        )

        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {"type": "Point", "coordinates": [49, 2, 100]},
                }
            ],
        }

        # first items
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4979&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        # Create test layer

        vl = QgsVectorLayer(
            "url='http://" + endpoint + "' typename='mycollection'", "test", "OAPIF"
        )
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.wkbType(), QgsWkbTypes.Type.PointZ)

        source = vl.dataProvider()

        app_log = QgsApplication.messageLog()
        # signals should be emitted by application log
        app_spy = QSignalSpy(app_log.messageReceived)

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/4979&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(items).encode("UTF-8"))

        features = [f for f in vl.getFeatures()]
        self.assertEqual(len(features), 1)

        self.assertEqual(len(app_spy), 0, list(app_spy))

    def testFeatureInsertionDeletion(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureInsertion"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        # Basic OPTIONS response: no AddFeatures capability
        self.assertEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().AddFeatures,
            vl.dataProvider().NoCapabilities,
        )

        # POST on /items, but no DELETE on /items/id
        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().AddFeatures,
            vl.dataProvider().NoCapabilities,
        )
        self.assertEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().DeleteFeatures,
            vl.dataProvider().NoCapabilities,
        )

        # DELETE on /items/id
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, DELETE")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().AddFeatures,
            vl.dataProvider().NoCapabilities,
        )
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().DeleteFeatures,
            vl.dataProvider().NoCapabilities,
        )

        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items?POSTDATA={"geometry":{"coordinates":[2.0,49.0],"type":"Point"},"properties":{"cnt":1234567890123,"pk":1},"type":"Feature"}',
            ),
            "wb",
        ) as f:
            f.write(b"Location: /collections/mycollection/items/new_id\r\n")

        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items?POSTDATA={"geometry":null,"properties":{"cnt":null,"pk":null},"type":"Feature"}',
            ),
            "wb",
        ) as f:
            # location in lower case to test fix for https://github.com/qgis/QGIS/issues/61729
            f.write(b"location: /collections/mycollection/items/other_id\r\n")

        new_id = {
            "type": "Feature",
            "id": "new_id",
            "properties": {"pk": 1, "cnt": 1234567890123},
            "geometry": {"type": "Point", "coordinates": [2, 49]},
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items/new_id?" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(new_id).encode("UTF-8"))

        other_id = {
            "type": "Feature",
            "id": "other_id",
            "properties": {"pk": 2, "cnt": 123},
            "geometry": None,
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items/other_id?" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(other_id).encode("UTF-8"))

        f = QgsFeature()
        f.setFields(vl.fields())
        f.setAttributes([None, 1, 1234567890123])
        f.setGeometry(QgsGeometry.fromWkt("Point (2 49)"))

        f2 = QgsFeature()
        f2.setFields(vl.fields())

        ret, fl = vl.dataProvider().addFeatures([f, f2])
        self.assertTrue(ret)

        self.assertEqual(fl[0].id(), 1)
        self.assertEqual(fl[0]["id"], "new_id")
        self.assertEqual(fl[0]["pk"], 1)
        self.assertEqual(fl[0]["cnt"], 1234567890123)

        self.assertEqual(fl[1].id(), 2)
        self.assertEqual(fl[1]["id"], "other_id")
        self.assertEqual(fl[1]["pk"], 2)
        self.assertEqual(fl[1]["cnt"], 123)

        # Failed attempt
        self.assertFalse(vl.dataProvider().deleteFeatures([1]))

        with open(
            sanitize(endpoint, "/collections/mycollection/items/new_id?VERB=DELETE"),
            "wb",
        ) as f:
            f.write(b"")

        with open(
            sanitize(endpoint, "/collections/mycollection/items/other_id?VERB=DELETE"),
            "wb",
        ) as f:
            f.write(b"")

        self.assertTrue(vl.dataProvider().deleteFeatures([1, 2]))

    def testFeatureInsertionNonDefaultCrs(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureInsertionNonDefaultCrs"
        )
        create_landing_page_api_collection(
            endpoint, storageCrs="http://www.opengis.net/def/crs/EPSG/0/4326"
        )

        # first items (lat, long) order
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        # POST on /items, but no DELETE on /items/id
        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        # (lat, long) order
        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items?POSTDATA={"geometry":{"coordinates":[49.0,2.0],"type":"Point"},"properties":{"cnt":1234567890123,"pk":1},"type":"Feature"}&Content-Crs=http://www.opengis.net/def/crs/EPSG/0/4326',
            ),
            "wb",
        ) as f:
            f.write(b"Location: /collections/mycollection/items/new_id\r\n")

        f = QgsFeature()
        f.setFields(vl.fields())
        f.setAttributes([None, 1, 1234567890123])
        f.setGeometry(QgsGeometry.fromWkt("Point (2 49)"))

        ret, _ = vl.dataProvider().addFeatures([f])
        self.assertTrue(ret)

    def testCurvedGeometryWrites(self):
        """Arcs are written as the JSON-FG "place", next to their linearized GeoJSON "geometry" """

        crs = "http://www.opengis.net/def/crs/EPSG/0/2056"
        arc = QgsGeometry.fromWkt(
            "CircularString (2600000 1200000, 2600001 1200001, 2600002 1200000)"
        )
        place = '"place":{"coordinates":[[2600000.0,1200000.0],[2600001.0,1200001.0],[2600002.0,1200000.0]],"type":"CircularString"}'
        conforms_to = '"conformsTo":["http://www.opengis.net/spec/json-fg-1/1.0/conf/core","http://www.opengis.net/spec/json-fg-1/1.0/conf/circular-arcs"]'
        coord_ref_sys = f'"coordRefSys":"{crs}"'

        for supports_patch in (False, True):
            endpoint = (
                self.__class__.basetestpath
                + f"/fake_qgis_http_endpoint_testCurvedGeometryWrites_{supports_patch}"
            )
            create_landing_page_api_collection(endpoint, storageCrs=crs)
            items = {
                "type": "FeatureCollection",
                "features": [
                    {
                        "type": "Feature",
                        "id": "feat.1",
                        "properties": {"name": "a"},
                        "geometry": {
                            "type": "LineString",
                            "coordinates": [[2600000, 1200000], [2600002, 1200000]],
                        },
                    }
                ],
            }
            for limit in (10, 1000):
                write_fake_response(
                    endpoint,
                    f"/collections/mycollection/items?limit={limit}&crs={crs}&"
                    + ACCEPT_ITEMS,
                    json.dumps(items).encode("UTF-8"),
                )
            write_fake_response(
                endpoint,
                "/collections/mycollection/items?VERB=OPTIONS",
                b"HEAD, GET, POST",
            )
            write_fake_response(
                endpoint,
                "/collections/mycollection/items/feat.1?VERB=OPTIONS",
                b"HEAD, GET, PUT, DELETE" + (b", PATCH" if supports_patch else b""),
            )

            vl = QgsVectorLayer(
                "url='http://" + endpoint + "' typename='mycollection'",
                "test",
                "OAPIF",
            )
            self.assertTrue(vl.isValid())
            self.assertEqual([f["name"] for f in vl.getFeatures()], ["a"])

            if supports_patch:
                write_fake_response(
                    endpoint,
                    "/collections/mycollection/items/feat.1?PATCHDATA={"
                    + f'{coord_ref_sys},"geometry":{arc.asJson()},{place}'
                    + f"}}&Content-Crs={crs}&Content-Type=application_merge-patch+json",
                    b"",
                )
                self.assertTrue(vl.dataProvider().changeGeometryValues({1: arc}))
                continue

            # the exporter writes 6 decimals
            geometry = f'"geometry":{arc.asJson(6)}'
            write_fake_response(
                endpoint,
                "/collections/mycollection/items/feat.1?PUTDATA={"
                + f'{conforms_to},{coord_ref_sys},{geometry},"id":"feat.1",{place},"properties":{{"name":"a"}},"type":"Feature"'
                + f"}}&Content-Crs={crs}",
                b"",
            )
            self.assertTrue(vl.dataProvider().changeGeometryValues({1: arc}))

            write_fake_response(
                endpoint,
                "/collections/mycollection/items?POSTDATA={"
                + f'{conforms_to},{coord_ref_sys},{geometry},{place},"properties":{{"name":"b"}},"type":"Feature"'
                + f"}}&Content-Crs={crs}",
                b"Location: /collections/mycollection/items/new_id\r\n",
            )
            f = QgsFeature(vl.fields())
            f.setAttribute("name", "b")
            f.setGeometry(arc)
            ret, _ = vl.dataProvider().addFeatures([f], QgsFeatureSink.Flag.FastInsert)
            self.assertTrue(ret)

    def testFeatureGeometryChange(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureGeometryChange"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, PUT, DELETE")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().ChangeGeometries,
            vl.dataProvider().NoCapabilities,
        )

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        values = [f.id() for f in vl.getFeatures()]
        self.assertEqual(values, [1])

        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items/feat.1?PUTDATA={"geometry":{"coordinates":[3.0,50.0],"type":"Point"},"id":"feat.1","properties":{"cnt":100,"pk":1},"type":"Feature"}',
            ),
            "wb",
        ) as f:
            f.write(b"")

        self.assertTrue(
            vl.dataProvider().changeGeometryValues(
                {1: QgsGeometry.fromWkt("Point (3 50)")}
            )
        )

        got_f = [f for f in vl.getFeatures()]
        got = got_f[0].geometry().constGet()
        self.assertEqual((got.x(), got.y()), (3.0, 50.0))

    def testFeatureGeometryChangeNonDefaultCrs(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureGeometryChangeNonDefaultCrs"
        )
        create_landing_page_api_collection(
            endpoint, storageCrs="http://www.opengis.net/def/crs/EPSG/0/4326"
        )

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, PUT, DELETE")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().ChangeGeometries,
            vl.dataProvider().NoCapabilities,
        )

        # real page
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        values = [f.id() for f in vl.getFeatures()]
        self.assertEqual(values, [1])

        # (Lat, Long) order
        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items/feat.1?PUTDATA={"geometry":{"coordinates":[50.0,3.0],"type":"Point"},"id":"feat.1","properties":{"cnt":100,"pk":1},"type":"Feature"}&Content-Crs=http://www.opengis.net/def/crs/EPSG/0/4326',
            ),
            "wb",
        ) as f:
            f.write(b"")

        self.assertTrue(
            vl.dataProvider().changeGeometryValues(
                {1: QgsGeometry.fromWkt("Point (3 50)")}
            )
        )

        got_f = [f for f in vl.getFeatures()]
        got = got_f[0].geometry().constGet()
        self.assertEqual((got.x(), got.y()), (3.0, 50.0))

    def testFeatureGeometryChangePatch(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureGeometryChangePatch"
        )
        create_landing_page_api_collection(
            endpoint, storageCrs="http://www.opengis.net/def/crs/EPSG/0/4326"
        )

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [-70.332, 66.33]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=10&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, PUT, DELETE, PATCH")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().ChangeGeometries,
            vl.dataProvider().NoCapabilities,
        )

        # real page
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/4326&"
                + ACCEPT_ITEMS,
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        values = [f.id() for f in vl.getFeatures()]
        self.assertEqual(values, [1])

        # (Lat, Long) order
        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items/feat.1?PATCHDATA={"geometry":{"coordinates":[50.0,3.0],"type":"Point"}}&Content-Crs=http://www.opengis.net/def/crs/EPSG/0/4326&Content-Type=application_merge-patch+json',
            ),
            "wb",
        ) as f:
            f.write(b"")

        self.assertTrue(
            vl.dataProvider().changeGeometryValues(
                {1: QgsGeometry.fromWkt("Point (3 50)")}
            )
        )

        got_f = [f for f in vl.getFeatures()]
        got = got_f[0].geometry().constGet()
        self.assertEqual((got.x(), got.y()), (3.0, 50.0))

    def testFeatureAttributeChange(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureAttributeChange"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, PUT, DELETE")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().ChangeGeometries,
            vl.dataProvider().NoCapabilities,
        )

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        values = [f.id() for f in vl.getFeatures()]
        self.assertEqual(values, [1])

        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items/feat.1?PUTDATA={"geometry":{"coordinates":[66.33,-70.332],"type":"Point"},"id":"feat.1","properties":{"cnt":200,"pk":1},"type":"Feature"}',
            ),
            "wb",
        ) as f:
            f.write(b"")

        self.assertTrue(vl.dataProvider().changeAttributeValues({1: {2: 200}}))

        values = [f["cnt"] for f in vl.getFeatures()]
        self.assertEqual(values, [200])

    def testFeatureAttributeChangePatch(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureAttributeChangePatch"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {"pk": 1, "cnt": 100},
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/feat.1?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, PUT, DELETE, PATCH")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().ChangeGeometries,
            vl.dataProvider().NoCapabilities,
        )

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        values = [f.id() for f in vl.getFeatures()]
        self.assertEqual(values, [1])

        with open(
            sanitize(
                endpoint,
                '/collections/mycollection/items/feat.1?PATCHDATA={"properties":{"cnt":200}}&Content-Type=application_merge-patch+json',
            ),
            "wb",
        ) as f:
            f.write(b"")

        self.assertTrue(vl.dataProvider().changeAttributeValues({1: {2: 200}}))

        values = [f["cnt"] for f in vl.getFeatures()]
        self.assertEqual(values, [200])

    # GDAL 3.5.0 is required since it is the first version that tags "complex"
    # fields as OFSTJSON
    @unittest.skipIf(
        int(gdal.VersionInfo("VERSION_NUM")) < GDAL_COMPUTE_VERSION(3, 5, 0),
        "GDAL 3.5.0 required",
    )
    def testFeatureComplexAttribute(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFeatureComplexAttribute"
        )
        create_landing_page_api_collection(endpoint)

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "center": {"type": "Point", "coordinates": [6.50, 51.80]}
                    },
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        self.assertEqual(vl.fields().field("center").type(), QVariant.Map)

        # First time we getFeatures(): comes directly from the GeoJSON layer
        values = [f["center"] for f in vl.getFeatures()]
        self.assertEqual(values, [{"coordinates": [6.5, 51.8], "type": "Point"}])

        # Now, that comes from the Spatialite cache, through
        # serialization and deserialization
        values = [f["center"] for f in vl.getFeatures()]
        self.assertEqual(values, [{"coordinates": [6.5, 51.8], "type": "Point"}])

    def testPart5Schema(self):
        """Base test for OGC API Features Part 5 - Schema"""

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testPart5Schema"
        )
        additionalConformance = [
            "http://www.opengis.net/spec/ogcapi-features-5/1.0/conf/schemas",
        ]
        collectionLinks = [
            {
                "type": "text/html",
                "rel": "http://www.opengis.net/def/rel/ogc/1.0/schema",
                "title": "Schema of collection in HTML",
                "href": "http://"
                + endpoint
                + "/collections/mycollection/schema?f=html",
            },
            {
                "type": "application/schema+json",
                "rel": "http://www.opengis.net/def/rel/ogc/1.0/schema",
                "title": "Schema of collection in JSON",
                "href": "http://"
                + endpoint
                + "/collections/mycollection/schema?f=json",
            },
        ]
        create_landing_page_api_collection(
            endpoint,
            additionalConformance=additionalConformance,
            collectionLinks=collectionLinks,
        )

        filename = sanitize(
            endpoint, "/collections/mycollection/schema?f=json&" + ACCEPT_SCHEMA
        )
        schema = {
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "$id": "http://" + endpoint + "/collections/mycollection/schema?f=json",
            "type": "object",
            "title": "My Collection",
            "properties": {
                "id": {
                    "title": "Feature identifier",
                    "description": "Long description",
                    "type": "string",
                    "readOnly": True,
                    "x-ogc-role": "id",
                    "x-ogc-propertySeq": 1,
                },
                "intfield": {"type": "integer", "x-ogc-propertySeq": 3},
                "strfield": {"type": "string", "x-ogc-propertySeq": 2},
                "doublefield": {"type": "number", "x-ogc-propertySeq": 4},
                "boolfield": {"type": "boolean", "x-ogc-propertySeq": 5},
                "datetimefield": {
                    "type": "string",
                    "format": "date-time",
                    "x-ogc-propertySeq": 6,
                },
                "datefield": {
                    "type": "string",
                    "format": "date",
                    "x-ogc-propertySeq": 7,
                },
                "geometry": {
                    "x-ogc-role": "primary-geometry",
                    "format": "geometry-point",
                    "x-ogc-propertySeq": 8,
                },
                "str2field": {"type": "string", "x-ogc-propertySeq": 9},
                "objectfield": {"type": "object", "x-ogc-propertySeq": 10},
                "arrayfield": {"type": "array", "x-ogc-propertySeq": 11},
            },
        }
        with open(filename, "wb") as f:
            f.write(json.dumps(schema).encode("UTF-8"))

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {
                        "intfield": 1,
                        "strfield": "foo",
                        "doublefield": 1.25,
                        "boolfield": True,
                        "datetimefield": "2025-03-17T17:13:00+0100",
                        "datefield": "2025-03-17",
                    },
                    "geometry": {"type": "Point", "coordinates": [66.33, -70.332]},
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        fields = vl.fields()
        self.assertEqual(len(fields), 10)
        self.assertEqual(
            [fields[i].name() for i in range(len(fields))],
            [
                "id",
                "strfield",
                "intfield",
                "doublefield",
                "boolfield",
                "datetimefield",
                "datefield",
                "str2field",
                "objectfield",
                "arrayfield",
            ],
        )
        self.assertEqual(fields[0].alias(), "Feature identifier")
        self.assertEqual(fields[0].comment(), "Long description")
        self.assertTrue(fields[0].isReadOnly())
        self.assertFalse(fields[1].isReadOnly())
        self.assertEqual(
            [fields[i].type() for i in range(len(fields))],
            [
                QMetaType.Type.QString,
                QMetaType.Type.QString,
                QMetaType.Type.LongLong,
                QMetaType.Type.Double,
                QMetaType.Type.Bool,
                QMetaType.Type.QDateTime,
                QMetaType.Type.QDate,
                QMetaType.Type.QString,
                QMetaType.Type.QVariantMap,
                QMetaType.Type.QVariantList,
            ],
        )
        self.assertEqual(vl.wkbType(), QgsWkbTypes.Type.Point)

        values = [
            [f["id"], f["strfield"], f["intfield"], f["doublefield"], f["boolfield"]]
            for f in vl.getFeatures()
        ]
        self.assertEqual(values, [["feat.1", "foo", 1, 1.25, True]])

        self.assertEqual(vl.dataProvider().geometryColumnName(), "geometry")

    def _testPart5SchemaSingleOrMulti(
        self, geometryFormat, geojsonGeom, expectedWkbType, expectedWkt
    ):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint__testPart5SchemaSingleOrMulti"
        )
        additionalConformance = [
            "http://www.opengis.net/spec/ogcapi-features-5/1.0/conf/schemas",
        ]
        collectionLinks = [
            {
                "type": "application/schema+json",
                "rel": "[ogc-rel:schema]",
                "title": "Schema of collection in JSON",
                "href": "http://" + endpoint + "/collections/mycollection/schema",
            }
        ]
        create_landing_page_api_collection(
            endpoint,
            additionalConformance=additionalConformance,
            collectionLinks=collectionLinks,
        )

        filename = sanitize(
            endpoint, "/collections/mycollection/schema?" + ACCEPT_SCHEMA
        )
        schema = {
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "$id": "http://" + endpoint + "/collections/mycollection/schema?f=json",
            "type": "object",
            "title": "My Collection",
            "properties": {
                "id": {"type": "string", "x-ogc-role": "id", "type": "string"},
                "geometry": {
                    "x-ogc-role": "primary-geometry",
                    "format": geometryFormat,
                },
            },
        }
        with open(filename, "wb") as f:
            f.write(json.dumps(schema).encode("UTF-8"))

        # first items
        first_items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {},
                    "geometry": geojsonGeom,
                }
            ],
        }
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=10&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        # real page
        with open(
            sanitize(
                endpoint, "/collections/mycollection/items?limit=1000&" + ACCEPT_ITEMS
            ),
            "wb",
        ) as f:
            f.write(json.dumps(first_items).encode("UTF-8"))

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' restrictToRequestBBOX=1",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.wkbType(), expectedWkbType)

        f = next(vl.getFeatures())
        self.assertEqual(f.geometry().wkbType(), expectedWkbType)
        self.assertEqual(f.geometry().asWkt().upper(), expectedWkt)

    def testPart5SchemaPointOrMultiPoint(self):
        """Test "format": "geometry-point-or-multipoint" """
        self._testPart5SchemaSingleOrMulti(
            "geometry-point-or-multipoint",
            {"type": "Point", "coordinates": [2, 49]},
            QgsWkbTypes.Type.MultiPoint,
            "MULTIPOINT ((2 49))",
        )

    def testPart5SchemaLineStringOrMultiLineString(self):
        """Test "format": "geometry-linestring-or-multilinestring" """
        self._testPart5SchemaSingleOrMulti(
            "geometry-linestring-or-multilinestring",
            {"type": "LineString", "coordinates": [[2, 49], [3, 50]]},
            QgsWkbTypes.Type.MultiLineString,
            "MULTILINESTRING ((2 49, 3 50))",
        )

    def testPart5SchemaPolygonOrMultiPolygon(self):
        """Test "format": "geometry-linestring-or-multilinestring" """
        self._testPart5SchemaSingleOrMulti(
            "geometry-polygon-or-multipolygon",
            {
                "type": "Polygon",
                "coordinates": [[[2, 49], [2, 50], [3, 50], [3, 49], [2, 49]]],
            },
            QgsWkbTypes.Type.MultiPolygon,
            "MULTIPOLYGON (((2 49, 2 50, 3 50, 3 49, 2 49)))",
        )

    def testFlatgeobufOutputFormat(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testFlatgeobufOutputFormat"
        )
        collectionLinks = [
            {
                "type": "application/flatgeobuf",
                "rel": "items",
                "title": "Items in FlatGeoBuf forma",
                "href": "http://" + endpoint + "/collections/mycollection/items?f=fgb",
            }
        ]
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=collectionLinks,
        )

        ds = gdal.GetDriverByName("FlatGeoBuf").Create(
            "/vsimem/tmp.fgb", 0, 0, 0, gdal.GDT_Unknown
        )
        lyr = ds.CreateLayer("tmp")
        lyr.CreateField(ogr.FieldDefn("id", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["id"] = "my_id"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (-70.5 66.5)"))
        lyr.CreateFeature(f)
        del ds

        f = gdal.VSIFOpenL("/vsimem/tmp.fgb", "rb")
        self.assertNotEqual(f, None)
        length = gdal.VSIStatL("/vsimem/tmp.fgb").size
        data = gdal.VSIFReadL(length, 1, f)
        gdal.VSIFCloseL(f)
        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=fgb&limit=10&Accept=application/flatgeobuf",
            ),
            "wb",
        ) as f:
            f.write(data)
        # Editing URLs must not be derived from the format specific items link
        with open(
            sanitize(endpoint, "/collections/mycollection/items?VERB=OPTIONS"), "wb"
        ) as f:
            f.write(b"HEAD, GET, POST")
        with open(
            sanitize(endpoint, "/collections/mycollection/items/my_id?VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET, PUT, DELETE")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' outputformat='application/flatgeobuf'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().AddFeatures,
            vl.dataProvider().NoCapabilities,
        )
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().ChangeAttributeValues,
            vl.dataProvider().NoCapabilities,
        )
        self.assertNotEqual(
            vl.dataProvider().capabilities() & vl.dataProvider().DeleteFeatures,
            vl.dataProvider().NoCapabilities,
        )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=fgb&limit=1000&Accept=application/flatgeobuf",
            ),
            "wb",
        ) as f:
            f.write(
                (
                    # Test bugfix for https://github.com/qgis/QGIS/issues/66365
                    # Note that the 2 links are folded in a pseudo-single one
                    # as QgsNetworkReply::rawHeaderPairs() does...
                    # Cf https://doc.qt.io/archives/qt-6.9/qnetworkreply.html#setRawHeader
                    "OGC-NumberMatched: 2\r\n"
                    + "Link: <http://"
                    + endpoint
                    + '/collections/mycollection/items?f=other_format&offset=next_offset&with=some,comma>; rel="next"; type="other_format"'
                    + ", "
                    + "<http://"
                    + endpoint
                    + '/collections/mycollection/items?f=fgb&offset=next_offset>; rel="next"; type="application/flatgeobuf"\r\n'
                    + "\r\n"
                ).encode("utf-8")
                + data
            )

        ds = gdal.GetDriverByName("FlatGeoBuf").Create(
            "/vsimem/tmp.fgb", 0, 0, 0, gdal.GDT_Unknown
        )
        lyr = ds.CreateLayer("tmp")
        lyr.CreateField(ogr.FieldDefn("id", ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["id"] = "my_id2"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (-70.25 66.25)"))
        lyr.CreateFeature(f)
        del ds

        f = gdal.VSIFOpenL("/vsimem/tmp.fgb", "rb")
        self.assertNotEqual(f, None)
        length = gdal.VSIStatL("/vsimem/tmp.fgb").size
        data = gdal.VSIFReadL(length, 1, f)
        gdal.VSIFCloseL(f)

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=fgb&offset=next_offset&Accept=application/flatgeobuf",
            ),
            "wb",
        ) as f:
            f.write((b"\r\n") + data)

        it = vl.getFeatures()
        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (-70.5 66.5)")
        my_id_fid = f.id()

        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (-70.25 66.25)")

        with open(
            sanitize(endpoint, "/collections/mycollection/items/my_id?VERB=DELETE"),
            "wb",
        ) as f:
            f.write(b"")
        self.assertTrue(vl.dataProvider().deleteFeatures([my_id_fid]))

    def _writeArrowItems(
        self, endpoint, pages, params="", next_link_type=ARROW_MEDIA_TYPE
    ):
        """Write the fixtures of the limit=10 request made when opening the layer,
        and of the download of pages, each linking to the next one"""
        items = "/collections/mycollection/items?f=arrow"
        write_fake_response(
            endpoint, items + "&limit=10" + params + "&" + ACCEPT_ARROW, pages[0]
        )
        for i, page in enumerate(pages):
            query = items + ("&limit=1000" + params if i == 0 else f"&offset={i}")
            headers = ""
            if i + 1 < len(pages):
                headers = f'Link: <http://{endpoint}{items}&offset={i + 1}>; rel="next"; type="{next_link_type}"\r\n'
            write_fake_response(
                endpoint,
                query + "&" + ACCEPT_ARROW,
                (headers + "\r\n").encode("utf-8") + page,
            )

    def _arrowLayer(self, endpoint, outputformat=ARROW_MEDIA_TYPE):
        uri = "url='http://" + endpoint + "' typename='mycollection'"
        if outputformat:
            uri += " outputformat='" + outputformat + "'"
        return QgsVectorLayer(uri, "test", "OAPIF")

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowOutputFormat(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowOutputFormat"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )
        fields = [("id", ogr.OFTString), ("name", ogr.OFTString)]
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    fields, [({"id": "feat.1", "name": "foo"}, "POINT (2 49)")]
                ),
                arrow_stream(
                    fields, [({"id": "feat.2", "name": "bar"}, "POINT (3 50)")]
                ),
            ],
            # spelled differently from the link of the collection
            next_link_type="Application/vnd.apache.arrow.stream; charset=binary",
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.fields().names(), ["id", "name"])

        features = list(vl.getFeatures())
        self.assertEqual([f["id"] for f in features], ["feat.1", "feat.2"])
        self.assertEqual([f["name"] for f in features], ["foo", "bar"])
        self.assertEqual(
            [f.geometry().asWkt() for f in features], ["Point (2 49)", "Point (3 50)"]
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowAxisOrder(self):
        """GeoArrow coordinates are longitude first, whatever the axis order of the CRS"""

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testArrowAxisOrder"
        )
        crs = "http://www.opengis.net/def/crs/EPSG/0/4326"
        create_landing_page_api_collection(
            endpoint, storageCrs=crs, collectionLinks=[arrow_items_link(endpoint)]
        )
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    [("id", ogr.OFTString)],
                    [({"id": "feat.1"}, "POINT (2 49)")],
                    srs="EPSG:4326",
                )
            ],
            params="&crs=" + crs,
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.sourceCrs().authid(), "EPSG:4326")
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()], ["Point (2 49)"]
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowWithoutIdColumn(self):
        """A column merely containing "id" in its name is not taken for the feature id"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowWithoutIdColumn"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    [("width", ogr.OFTInteger)],
                    [({"width": 10}, "POINT (2 49)"), ({"width": 10}, "POINT (3 50)")],
                )
            ],
        )
        write_fake_response(
            endpoint, "/collections/mycollection/items?VERB=OPTIONS", b"HEAD, GET, POST"
        )
        # The id hashed from the content of a feature can't address it
        write_fake_response(
            endpoint,
            "/collections/mycollection/items/?VERB=OPTIONS",
            b"HEAD, GET, PUT, DELETE",
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        capabilities = vl.dataProvider().capabilities()
        self.assertNotEqual(
            capabilities & vl.dataProvider().AddFeatures,
            vl.dataProvider().NoCapabilities,
        )
        self.assertEqual(
            capabilities & vl.dataProvider().ChangeAttributeValues,
            vl.dataProvider().NoCapabilities,
        )
        self.assertEqual(
            capabilities & vl.dataProvider().DeleteFeatures,
            vl.dataProvider().NoCapabilities,
        )
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()],
            ["Point (2 49)", "Point (3 50)"],
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowColumnAliasedId(self):
        """A column merely aliased "id" is not taken for the feature id"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowColumnAliasedId"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    [("category", ogr.OFTString)],
                    [
                        ({"category": "same"}, "POINT (2 49)"),
                        ({"category": "same"}, "POINT (3 50)"),
                    ],
                    alternative_names={"category": "ID"},
                )
            ],
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.fields().field("category").alias(), "ID")
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()],
            ["Point (2 49)", "Point (3 50)"],
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowDictionaryColumn(self):
        """Every page is a stream with its own dictionary"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowDictionaryColumn"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )

        def page(values, features):
            domain = ogr.CreateCodedFieldDomain(
                "landuseDomain", "", ogr.OFTInteger, ogr.OFSTNone, values
            )
            return arrow_stream(
                [("id", ogr.OFTString), ("landuse", ogr.OFTInteger, "landuseDomain")],
                [(attributes, "POINT (2 49)") for attributes in features],
                domains=[domain],
            )

        self._writeArrowItems(
            endpoint,
            [
                page(
                    {0: "residential", 1: "commercial"},
                    # a null code is not the one of the first value
                    [{"id": "feat.1", "landuse": 1}, {"id": "feat.2"}],
                ),
                page(
                    {0: "commercial", 1: "industrial"},
                    [{"id": "feat.3", "landuse": 1}],
                ),
            ],
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.fields().field("landuse").type(), QMetaType.Type.QString)
        self.assertEqual(
            [f["landuse"] for f in vl.getFeatures()],
            ["commercial", NULL, "industrial"],
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowNullIntegerIds(self):
        """Features whose integer id is null are not all taken for the feature of id 0"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowNullIntegerIds"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    [("id", ogr.OFTInteger), ("name", ogr.OFTString)],
                    [
                        ({"name": "a"}, "POINT (2 49)"),
                        ({"name": "b"}, "POINT (3 50)"),
                        ({"id": 0, "name": "c"}, "POINT (4 51)"),
                    ],
                )
            ],
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(sorted(f["name"] for f in vl.getFeatures()), ["a", "b", "c"])

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowTruncatedStream(self):
        """A stream cut inside a record batch opens, but must not pass for a page without features"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowTruncatedStream"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )
        page = arrow_stream(
            [("id", ogr.OFTString)],
            [({"id": f"feat.{i}"}, f"POINT ({i} 49)") for i in range(100)],
        )
        self._writeArrowItems(endpoint, [page[:-100]])

        vl = self._arrowLayer(endpoint)
        self.assertFalse(vl.isValid())

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowObjectPropertyOfPart5Schema(self):
        """GDAL flattens an Arrow struct column into one field per member"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowObjectPropertyOfPart5Schema"
        )
        create_landing_page_api_collection(
            endpoint,
            additionalConformance=[
                "http://www.opengis.net/spec/ogcapi-features-5/1.0/conf/schemas"
            ],
            collectionLinks=[
                arrow_items_link(endpoint),
                {
                    "type": "application/schema+json",
                    "rel": "http://www.opengis.net/def/rel/ogc/1.0/schema",
                    "href": "http://"
                    + endpoint
                    + "/collections/mycollection/schema?f=json",
                },
            ],
        )
        schema = {
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "type": "object",
            "properties": {
                "id": {"type": "string", "x-ogc-role": "id", "x-ogc-propertySeq": 1},
                "address": {"type": "object", "x-ogc-propertySeq": 2},
                "geometry": {
                    "x-ogc-role": "primary-geometry",
                    "format": "geometry-point",
                    "x-ogc-propertySeq": 3,
                },
            },
        }
        write_fake_response(
            endpoint,
            "/collections/mycollection/schema?f=json&" + ACCEPT_SCHEMA,
            json.dumps(schema).encode("UTF-8"),
        )
        # The names GDAL gives to the members of a struct column
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    [
                        ("id", ogr.OFTString),
                        ("address.street", ogr.OFTString),
                        ("address.city", ogr.OFTString),
                    ],
                    [
                        (
                            {
                                "id": "feat.1",
                                "address.street": "Main",
                                "address.city": "Bern",
                            },
                            "POINT (2 49)",
                        ),
                        ({"id": "feat.2"}, "POINT (3 50)"),
                    ],
                )
            ],
        )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.fields().names(), ["id", "address"])
        self.assertEqual(
            [f["address"] for f in vl.getFeatures()],
            [{"street": "Main", "city": "Bern"}, NULL],
        )

    @unittest.skipIf(ARROW_USABLE, "only meaningful without a usable Arrow driver")
    def testArrowOutputFormatWithoutUsableDriver(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowOutputFormatWithoutUsableDriver"
        )
        create_landing_page_api_collection(
            endpoint, collectionLinks=[arrow_items_link(endpoint)]
        )
        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {},
                    "geometry": {"type": "Point", "coordinates": [2, 49]},
                }
            ],
        }
        for limit in (10, 1000):
            write_fake_response(
                endpoint,
                f"/collections/mycollection/items?limit={limit}&" + ACCEPT_ITEMS,
                json.dumps(items).encode("UTF-8"),
            )

        vl = self._arrowLayer(endpoint)
        self.assertTrue(vl.isValid())
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()], ["Point (2 49)"]
        )

    def _createGeoJSONAndArrowCollection(self, endpoint):
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=[
                {
                    "type": "application/geo+json",
                    "rel": "items",
                    "href": "http://" + endpoint + "/collections/mycollection/items",
                },
                arrow_items_link(endpoint),
            ],
        )

    def _writeGeoJSONItems(self, endpoint):
        items = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": "feat.1",
                    "properties": {},
                    "geometry": {"type": "Point", "coordinates": [2, 49]},
                }
            ],
        }
        for limit in (10, 1000):
            write_fake_response(
                endpoint,
                f"/collections/mycollection/items?limit={limit}&" + ACCEPT_ITEMS,
                json.dumps(items).encode("UTF-8"),
            )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowAutoSelect(self):

        endpoint = (
            self.__class__.basetestpath + "/fake_qgis_http_endpoint_testArrowAutoSelect"
        )
        self._createGeoJSONAndArrowCollection(endpoint)
        # Only the Arrow items are served: a layer reading GeoJSON would be invalid
        self._writeArrowItems(
            endpoint,
            [
                arrow_stream(
                    [("id", ogr.OFTString)], [({"id": "feat.1"}, "POINT (2 49)")]
                )
            ],
        )

        self.assertTrue(
            QgsSettingsTree.node("wfs")
            .childSetting("oapif-prefer-arrow")
            .valueAsVariant()
        )
        vl = self._arrowLayer(endpoint, outputformat=None)
        self.assertTrue(vl.isValid())
        self.assertEqual(
            [(f["id"], f.geometry().asWkt()) for f in vl.getFeatures()],
            [("feat.1", "Point (2 49)")],
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowAutoSelectOptOut(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowAutoSelectOptOut"
        )
        self._createGeoJSONAndArrowCollection(endpoint)
        self._writeGeoJSONItems(endpoint)

        setting = QgsSettingsTree.node("wfs").childSetting("oapif-prefer-arrow")
        setting.setVariantValue(False)
        self.addCleanup(setting.remove)
        vl = self._arrowLayer(endpoint, outputformat=None)
        self.assertTrue(vl.isValid())
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()], ["Point (2 49)"]
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowAutoSelectKeepsRequestedFormat(self):
        """A format asked for but not offered falls back to GeoJSON, not to Arrow"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowAutoSelectKeepsRequestedFormat"
        )
        self._createGeoJSONAndArrowCollection(endpoint)
        self._writeGeoJSONItems(endpoint)

        vl = self._arrowLayer(endpoint, outputformat="application/flatgeobuf")
        self.assertTrue(vl.isValid())
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()], ["Point (2 49)"]
        )

    @unittest.skipIf(ARROW_USABLE, "only meaningful without a usable Arrow driver")
    def testArrowAutoSelectWithoutUsableDriver(self):
        """An advertised Arrow link is not selected when GDAL can't read it"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testArrowAutoSelectWithoutUsableDriver"
        )
        self._createGeoJSONAndArrowCollection(endpoint)
        self._writeGeoJSONItems(endpoint)

        vl = self._arrowLayer(endpoint, outputformat=None)
        self.assertTrue(vl.isValid())
        self.assertEqual(
            [f.geometry().asWkt() for f in vl.getFeatures()], ["Point (2 49)"]
        )

    @unittest.skipIf(not ARROW_USABLE, "GDAL >= 3.8 with the Arrow driver required")
    def testArrowEditing(self):
        """Edits go to /items and /items/{id}, not to the Arrow link of the collection"""

        for supports_patch in (False, True):
            endpoint = (
                self.__class__.basetestpath
                + f"/fake_qgis_http_endpoint_testArrowEditing_{supports_patch}"
            )
            self._createGeoJSONAndArrowCollection(endpoint)
            self._writeArrowItems(
                endpoint,
                [
                    arrow_stream(
                        [("id", ogr.OFTString), ("name", ogr.OFTString)],
                        [({"id": "feat.1", "name": "foo"}, "POINT (2 49)")],
                    )
                ],
            )
            write_fake_response(
                endpoint,
                "/collections/mycollection/items?VERB=OPTIONS",
                b"HEAD, GET, POST",
            )
            write_fake_response(
                endpoint,
                "/collections/mycollection/items/feat.1?VERB=OPTIONS",
                b"HEAD, GET, PUT, DELETE" + (b", PATCH" if supports_patch else b""),
            )

            vl = self._arrowLayer(endpoint, outputformat=None)
            self.assertTrue(vl.isValid())
            self.assertEqual([f["id"] for f in vl.getFeatures()], ["feat.1"])

            if supports_patch:
                write_fake_response(
                    endpoint,
                    '/collections/mycollection/items/feat.1?PATCHDATA={"properties":{"name":"bar"}}&Content-Type=application_merge-patch+json',
                    b"",
                )
                self.assertTrue(
                    vl.dataProvider().changeAttributeValues({1: {1: "bar"}})
                )
                continue

            write_fake_response(
                endpoint,
                '/collections/mycollection/items/feat.1?PUTDATA={"geometry":{"coordinates":[3.0,50.0],"type":"Point"},"id":"feat.1","properties":{"name":"foo"},"type":"Feature"}',
                b"",
            )
            self.assertTrue(
                vl.dataProvider().changeGeometryValues(
                    {1: QgsGeometry.fromWkt("Point (3 50)")}
                )
            )

            write_fake_response(
                endpoint,
                '/collections/mycollection/items?POSTDATA={"geometry":{"coordinates":[4.0,51.0],"type":"Point"},"properties":{"name":"new"},"type":"Feature"}',
                b"Location: /collections/mycollection/items/new_id\r\n",
            )
            write_fake_response(
                endpoint,
                "/collections/mycollection/items/new_id?" + ACCEPT_ITEMS,
                json.dumps(
                    {
                        "type": "Feature",
                        "id": "new_id",
                        "properties": {"name": "from server"},
                        "geometry": {"type": "Point", "coordinates": [4, 51]},
                    }
                ).encode("UTF-8"),
            )
            f = QgsFeature(vl.fields())
            f.setAttribute("name", "new")
            f.setGeometry(QgsGeometry.fromWkt("Point (4 51)"))
            ret, features = vl.dataProvider().addFeatures([f])
            self.assertTrue(ret)
            # refreshed from /items/new_id
            self.assertEqual(features[0]["name"], "from server")

    def _testJsonFG_oapif1_1_OutputFormat(self, profile, profile_in_next_link=True):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testJSONFG_oapif1_1_OutputFormat"
        )
        collectionLinks = [
            {
                "type": "application/geo+json",
                "rel": "items",
                "title": "Items in JSON-FG format",
                "profile": [profile],
                "href": "http://"
                + endpoint
                + "/collections/mycollection/items?f=jsonfg",
            }
        ]
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=collectionLinks,
        )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=jsonfg&limit=10&Accept=application/fg+json",
            ),
            "wb",
        ) as f:
            f.write(
                json.dumps(
                    {
                        "type": "FeatureCollection",
                        "features": [
                            {
                                "type": "Feature",
                                "id": 1,
                                "properties": {},
                                "geometry": {
                                    "type": "Point",
                                    "coordinates": [-70.5, 66.5],
                                },
                            }
                        ],
                    }
                ).encode("UTF-8")
            )
        with open(
            sanitize(endpoint, "/collections/mycollection/items?f=jsonfg&VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"HEAD, GET")

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' outputformat='application/fg+json'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=jsonfg&limit=1000&Accept=application/fg+json",
            ),
            "wb",
        ) as f:
            profile_str = f'; profile="{profile}"' if profile_in_next_link else ""
            headers = (
                "OGC-NumberMatched: 2\r\nLink: <http://"
                + endpoint
                + '/collections/mycollection/items?f=jsonfg&offset=next_offset>; rel="next"; type="application/geo+json"'
                + profile_str
                + "\r\n\r\n"
            )
            f.write(
                headers.encode("utf-8")
                + json.dumps(
                    {
                        "type": "FeatureCollection",
                        "features": [
                            {
                                "type": "Feature",
                                "id": 2,
                                "properties": {},
                                "geometry": {
                                    "type": "Point",
                                    "coordinates": [-70.5, 66.5],
                                },
                            }
                        ],
                    }
                ).encode("UTF-8")
            )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=jsonfg&offset=next_offset&Accept=application/fg+json",
            ),
            "wb",
        ) as f:
            f.write(
                (b"\r\n")
                + json.dumps(
                    {
                        "type": "FeatureCollection",
                        "features": [
                            {
                                "type": "Feature",
                                "properties": {},
                                "geometry": {
                                    "type": "Point",
                                    "coordinates": [-70.25, 66.25],
                                },
                            }
                        ],
                    }
                ).encode("UTF-8")
            )

        it = vl.getFeatures()
        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (-70.5 66.5)")

        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (-70.25 66.25)")

    def testJsonFG_oapif1_1_OutputFormat_profile_jsonfg(self):
        self._testJsonFG_oapif1_1_OutputFormat(
            "http://www.opengis.net/def/profile/ogc/0/jsonfg"
        )

    def testJsonFG_oapif1_1_OutputFormat_profile_jsonfg_without_profile_in_next_link(
        self,
    ):
        self._testJsonFG_oapif1_1_OutputFormat(
            "http://www.opengis.net/def/profile/ogc/0/jsonfg",
            profile_in_next_link=False,
        )

    def testJsonFG_oapif1_1_OutputFormat_profile_jsonfg_plus(self):
        self._testJsonFG_oapif1_1_OutputFormat(
            "http://www.opengis.net/def/profile/ogc/0/jsonfg-plus"
        )

    def testGMLOutputFormat_no_xml_schema_paging(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testGMLOutputFormat_no_xml_schema_paging"
        )
        collectionLinks = [
            {
                "type": "application/gml+xml",
                "rel": "items",
                "title": "Items in GML format",
                "href": "http://" + endpoint + "/collections/mycollection/items?f=gml",
            }
        ]
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=collectionLinks,
        )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=10&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            f.write(
                b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:intfield>1</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            )

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' outputformat='application/gml+xml'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=1000&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            data = b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:intfield>1</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            f.write(
                (
                    "OGC-NumberMatched: 2\r\nLink: <http://"
                    + endpoint
                    + '/collections/mycollection/items?f=gml&offset=next_offset>; rel="next"; type="application/gml+xml"\r\n\r\n'
                ).encode("utf-8")
                + data
            )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&offset=next_offset&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            data = b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.2">
      <my:intfield>2</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.2"><gml:pos>50 3</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            f.write(b"\r\n" + data)

        it = vl.getFeatures()

        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (2 49)")
        self.assertEqual(f["intfield"], 1)

        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (3 50)")
        self.assertEqual(f["intfield"], 2)

    def testGMLOutputFormat_no_xml_schema_but_bulk_download(self):

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testGMLOutputFormat_no_xml_schema"
        )
        collectionLinks = [
            {
                "type": "application/gml+xml",
                "rel": "items",
                "title": "Items in GML format",
                "href": "http://" + endpoint + "/collections/mycollection/items?f=gml",
            },
            {
                "type": "application/gml+xml",
                "rel": "enclosure",
                "title": "Bulk download in GML format",
                "href": "http://"
                + endpoint
                + "/collections/mycollection/items?bulk=yes",
            },
        ]
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=collectionLinks,
        )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=10&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            data = b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:intfield>1</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            f.write(data)

        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' outputformat='application/gml+xml'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?bulk=yes&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            f.write(
                b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:intfield>1</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            )

        it = vl.getFeatures()
        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (2 49)")
        self.assertEqual(f["intfield"], 1)

    def testGMLOutputFormat_with_simple_xml_schema(self):
        """Test parsing of simple feature schema with QGIS own schema parser"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testGMLOutputFormat_with_simple_xml_schema"
        )
        collectionLinks = [
            {
                "type": "application/gml+xml",
                "rel": "items",
                "title": "Items in GML format",
                "href": "http://" + endpoint + "/collections/mycollection/items?f=gml",
            },
            {
                "type": "application/xml",
                "rel": "describedby",
                "title": "XML schema",
                "href": "http://"
                + endpoint
                + "/collections/mycollection/my_schema.xsd?",
            },
        ]
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=collectionLinks,
        )

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=10&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            data = b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:intfield>1</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            f.write(data)

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/my_schema.xsd?",
            ),
            "wb",
        ) as f:
            f.write(
                b"""
<xsd:schema xmlns:my="http://my" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://my">
  <xsd:import namespace="http://www.opengis.net/gml"/>
  <xsd:complexType name="mycollectionType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="intfield" nillable="true" type="xsd:int"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="otherfield" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="geometry" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="mycollection" substitutionGroup="gml:_Feature" type="my:mycollectionType"/>
</xsd:schema>
"""
            )
        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' outputformat='application/gml+xml'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=1000&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            f.write(
                b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:intfield>1</my:intfield>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            )

        it = vl.getFeatures()
        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (2 49)")
        self.assertEqual(f["intfield"], 1)
        self.assertEqual(f["otherfield"], NULL)

    @unittest.skipIf(gdal.GetDriverByName("GMLAS") is None, "OGR GMLAS driver required")
    def testGMLOutputFormat_with_complex_xml_schema(self):
        """Test reading complex features"""

        endpoint = (
            self.__class__.basetestpath
            + "/fake_qgis_http_endpoint_testGMLOutputFormat_with_complex_xml_schema"
        )
        collectionLinks = [
            {
                "type": "application/gml+xml",
                "rel": "items",
                "title": "Items in GML format",
                "href": "http://" + endpoint + "/collections/mycollection/items?f=gml",
            },
            {
                "type": "application/xml",
                "rel": "describedby",
                "title": "XML schema",
                "href": "http://"
                + endpoint
                + "/collections/mycollection/my_schema.xsd?",
            },
        ]
        create_landing_page_api_collection(
            endpoint,
            collectionLinks=collectionLinks,
        )

        with open(
            sanitize(endpoint, "/collections/mycollection/items?f=gml&VERB=OPTIONS"),
            "wb",
        ) as f:
            f.write(b"GET")

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=10&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            data = b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:a><my:intfield>1</my:intfield></my:a>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            f.write(data)

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/my_schema.xsd?",
            ),
            "wb",
        ) as f:
            f.write(
                b"""<xsd:schema xmlns:my="http://my" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://my">
  <xsd:import namespace="http://www.opengis.net/gml/3.2" schemaLocation="https://schemas.opengis.net/gml/3.2.1/gml.xsd"/>
  <xsd:complexType name="mycollectionType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element name="a" maxOccurs="1" minOccurs="0" type="my:subType"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="geometry" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="mycollection" substitutionGroup="gml:_Feature" type="my:mycollectionType"/>
  <xsd:complexType name="subType">
    <xsd:sequence>
      <xsd:element maxOccurs="1" minOccurs="0" name="intfield" nillable="true" type="xsd:int"/>
    </xsd:sequence>
  </xsd:complexType>
</xsd:schema>
"""
            )
        vl = QgsVectorLayer(
            "url='http://"
            + endpoint
            + "' typename='mycollection' outputformat='application/gml+xml'",
            "test",
            "OAPIF",
        )
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.wkbType(), QgsWkbTypes.Type.Point)

        with open(
            sanitize(
                endpoint,
                "/collections/mycollection/items?f=gml&limit=1000&Accept=application/gml+xml",
            ),
            "wb",
        ) as f:
            f.write(
                b"""<foo:FeatureCollection xmlns:foo="http://example.com"
                       xmlns:gml="http://www.opengis.net/gml/3.2"
                       xmlns:my="http://my">
  <foo:featureMember>
    <my:mycollection gml:id="mycollection.1">
      <my:a><my:intfield>1</my:intfield></my:a>
      <my:geometry><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="geom.0"><gml:pos>49 2</gml:pos></gml:Point></my:geometry>
    </my:mycollection>
  </foo:featureMember>
</foo:FeatureCollection>"""
            )

        self.assertEqual(
            [f.name() for f in vl.fields()],
            [
                "id",
                "metadataproperty",
                "description_href",
                "description_title",
                "description_nilreason",
                "description",
                "descriptionreference_href",
                "descriptionreference_title",
                "descriptionreference_nilreason",
                "identifier_codespace",
                "identifier",
                "name",
                "location_location",
                "a_intfield",
                "a_intfield_nil",
            ],
        )
        it = vl.getFeatures()
        f = next(it)
        self.assertEqual(f.geometry().wkbType(), QgsWkbTypes.Type.Point)
        self.assertEqual(f.geometry().asWkt().upper(), "POINT (2 49)")
        self.assertEqual(f["a_intfield"], 1)


if __name__ == "__main__":
    unittest.main()
