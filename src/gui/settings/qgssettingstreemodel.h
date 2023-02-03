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

class GUI_EXPORT QgsSettingsTreeNodeData : public QObject
{
    Q_OBJECT
  public:

    //! Type of tree element
    enum class Type
    {
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

    QString name() const {return mName;}

    QString value() const {return mValue;}

    //! Returns if the setting exists (value is set)
    bool exists() const {return mExists;}

  private:
    QgsSettingsTreeNodeData( QObject *parent ) : QObject( parent ) {}
    void addChildForTreeNode( const QgsSettingsTreeNode *node );
    void addChildForNamedListItemNode( const QString &item, const QgsSettingsTreeNamedListNode *namedListNode );
    void addChildForSetting( const QgsSettingsEntryBase *setting );
    void fillChildren();

    QString mName;
    QString mValue;
    QStringList mNamedParentNodes;
    bool mExists = false;

    QList<QgsSettingsTreeNodeData *> mChildren;
    QgsSettingsTreeNodeData *mParent = nullptr;

    const QgsSettingsTreeNode *mTreeNode = nullptr;
    const QgsSettingsEntryBase *mSetting = nullptr;
};

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
