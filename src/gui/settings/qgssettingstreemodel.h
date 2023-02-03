/***************************************************************************
  qgssettingstreemodel.h
  --------------------------------------
  Date                 : January 2023
  Copyright            : (C) 2023 by Denis Rouzaud
  Email                : denis@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSETTINGSTREEMODEL_H
#define QGSSETTINGSTREEMODEL_H


#include "qgis_gui.h"

#include <QAbstractItemModel>

class QgsSettingsEntryBase;
class QgsSettingsTreeNode;
class QgsSettingsTreeNamedListNode;

#ifndef SIP_RUN

///@cond PRIVATE

/**
 * \ingroup gui
 * \class QgsSettingsTreeNodeData
 * \brief QgsSettingsTree holds data of the tree model for the settings tree.
 *
 * \note Not available in Python bindings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsTreeNodeData : public QObject
{
    Q_OBJECT
  public:

    //! Type of tree element
    enum class Type
    {
      RootNode,
      TreeNode,
      NamedListTreeNode,
      NamedListItem,
      Setting
    };

    static QgsSettingsTreeNodeData *createRootNodeData( const QgsSettingsTreeNode *rootNode, QObject *parent );

    bool isRoot() const {return mParent == nullptr;}

    QStringList namedParentNodes() const {return mNamedParentNodes;}

    QList<QgsSettingsTreeNodeData *> children() const {return mChildren;}

    QgsSettingsTreeNodeData *parent() const {return mParent;}

    Type type() const {return mType;}

    QString name() const {return mName;}

    QVariant value() const {return mValue;}

    //! Returns if the setting exists (value is set)
    bool exists() const {return mExists;}

    /**
     * Returns the setting of the node
     * It returns a nullptr if the setting does not exist
     */
    const QgsSettingsEntryBase *setting() const {return mSetting;}

  private:
    QgsSettingsTreeNodeData( QObject *parent ) : QObject( parent ) {}
    void addChildForTreeNode( const QgsSettingsTreeNode *node );
    void addChildForNamedListItemNode( const QString &item, const QgsSettingsTreeNamedListNode *namedListNode );
    void addChildForSetting( const QgsSettingsEntryBase *setting );
    void fillChildren();

    Type mType = Type::TreeNode;
    QString mName;
    QVariant mValue;
    QStringList mNamedParentNodes;
    bool mExists = false;

    QList<QgsSettingsTreeNodeData *> mChildren;
    QgsSettingsTreeNodeData *mParent = nullptr;

    const QgsSettingsTreeNode *mTreeNode = nullptr;
    const QgsSettingsEntryBase *mSetting = nullptr;
};

///@endcond

#endif



/**
 * \ingroup gui
 * \class QgsSettingsTreeModel
 * \brief QgsSettingsTreeModel is a tree model for the settings tree.
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsTreeModel : public QAbstractItemModel
{
  public:

    enum class Column
    {
      Name,
      Value,
    };

    QgsSettingsTreeModel( QgsSettingsTreeNode *rootNode = nullptr, QObject *parent = nullptr );

    ~QgsSettingsTreeModel();

    /**
     * Returns settings tree node for given index. Returns root node for invalid index.
     */
    QgsSettingsTreeNodeData *index2node( const QModelIndex &index ) const;

    QModelIndex index( int row, int column, const QModelIndex &parent ) const override;
    QModelIndex parent( const QModelIndex &child ) const override;
    int rowCount( const QModelIndex &parent ) const override;
    int columnCount( const QModelIndex &parent ) const override;
    QVariant data( const QModelIndex &index, int role ) const override;

  private:
    QModelIndex indexOfParentSettingsTreeNode( QgsSettingsTreeNodeData *parentNode ) const;

    QgsSettingsTreeNodeData *mRootNode = nullptr;
};

#endif // QGSSETTINGSTREEMODEL_H
