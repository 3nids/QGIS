/***************************************************************************
  qgssettingstreemodel.cpp
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

#include <QFont>

#include "qgssettingstreemodel.h"
#include "qgssettingsentry.h"
#include "qgssettingstreenode.h"
#include "qgssettingseditorwidgetwrapper.h"
#include "qgssettingseditorwidgetregistry.h"
#include "qgsgui.h"
#include "qgslogger.h"


QgsSettingsTreeNodeData *QgsSettingsTreeNodeData::createRootNodeData( const QgsSettingsTreeNode *rootNode, QObject *parent = nullptr )
{
  QgsSettingsTreeNodeData *nodeData = new QgsSettingsTreeNodeData( parent );
  nodeData->mType = Type::RootNode;
  nodeData->mName = rootNode->key();
  nodeData->mTreeNode = rootNode;
  nodeData->fillChildren();
  return nodeData;
}

void QgsSettingsTreeNodeData::updateSettingNode()
{
  Q_ASSERT( type() == QgsSettingsTreeNodeData::Type::Setting );
  int index = mParent->mChildren.indexOf( this );
  mParent->mChildren.removeAt( index );
  addChildForSetting( mSetting );
  // move from last row to the correct row
  mParent->mChildren.move( mParent->mChildren.count() - 1, index );
  deleteLater();
};



void QgsSettingsTreeNodeData::addChildForTreeNode( const QgsSettingsTreeNode *node )
{
  QgsSettingsTreeNodeData *nodeData = new QgsSettingsTreeNodeData( this );
  nodeData->mParent = this;
  nodeData->mNamedParentNodes = mNamedParentNodes;
  nodeData->mName = node->key();
  nodeData->mTreeNode = node;
  if ( node->type() == Qgis::SettingsTreeNodeType::NamedList )
  {
    nodeData->mType = Type::NamedListTreeNode;
    const QgsSettingsTreeNamedListNode *nln = dynamic_cast<const QgsSettingsTreeNamedListNode *>( node );
    const QStringList items = nln->items( mNamedParentNodes );
    for ( const QString &item : items )
    {
      nodeData->addChildForNamedListItemNode( item, nln );
    }
  }
  else
  {
    nodeData->mType = Type::TreeNode;
    nodeData->fillChildren();
  }
  mChildren.append( nodeData );
}

void QgsSettingsTreeNodeData::addChildForNamedListItemNode( const QString &item, const QgsSettingsTreeNamedListNode *namedListNode )
{
  QgsSettingsTreeNodeData *nodeData = new QgsSettingsTreeNodeData( this );
  nodeData->mType = Type::NamedListItem;
  nodeData->mParent = this;
  nodeData->mNamedParentNodes = mNamedParentNodes;
  nodeData->mNamedParentNodes.append( item );
  nodeData->mName = item;
  nodeData->mTreeNode = namedListNode;
  nodeData->fillChildren();
  mChildren.append( nodeData );
}

void QgsSettingsTreeNodeData::addChildForSetting( const QgsSettingsEntryBase *setting )
{
  QgsSettingsTreeNodeData *nodeData = new QgsSettingsTreeNodeData( this );
  nodeData->mType = Type::Setting;
  nodeData->mParent = this;
  nodeData->mNamedParentNodes = mNamedParentNodes;
  nodeData->mSetting = setting;
  nodeData->mName = setting->name();
  nodeData->mValue = setting->valueAsVariant( mNamedParentNodes );
  nodeData->mExists = setting->exists( mNamedParentNodes );

  switch ( mNamedParentNodes.count() )
  {
    case 1:
      QgsDebugMsg( QString( "getting %1 with %2" ).arg( setting->definitionKey(), mNamedParentNodes.at( 0 ) ) );
      break;
    case 2:
      QgsDebugMsg( QString( "getting %1 with %2" ).arg( setting->definitionKey(), mNamedParentNodes.at( 0 ), mNamedParentNodes.at( 1 ) ) );
      break;
    case 0:
      QgsDebugMsg( QString( "getting %1" ).arg( setting->definitionKey() ) );
      break;
    default:
      Q_ASSERT( false );
      QgsDebugMsg( QString( "Not handling that many named parent nodes for %1" ).arg( setting->definitionKey() ) );
      break;
  }

  mChildren.append( nodeData );
}

void QgsSettingsTreeNodeData::fillChildren()
{
  const QList<QgsSettingsTreeNode *> childrenNodes = mTreeNode->childrenNodes();
  for ( const QgsSettingsTreeNode *childNode : childrenNodes )
  {
    addChildForTreeNode( childNode );
  }
  const QList<const QgsSettingsEntryBase *> childrenSettings = mTreeNode->childrenSettings();
  for ( const QgsSettingsEntryBase *setting : childrenSettings )
  {
    addChildForSetting( setting );
  }
}



QgsSettingsTreeModel::QgsSettingsTreeModel( QgsSettingsTreeNode *rootNode, QObject *parent )
  : QAbstractItemModel( parent )
{
  mRootNode = QgsSettingsTreeNodeData::createRootNodeData( rootNode, this );
}

QgsSettingsTreeModel::~QgsSettingsTreeModel()
{
  //delete mRootNode;
}

QgsSettingsTreeNodeData *QgsSettingsTreeModel::index2node( const QModelIndex &index ) const
{
  if ( !index.isValid() )
    return mRootNode;

  QObject *obj = reinterpret_cast<QObject *>( index.internalPointer() );
  return qobject_cast<QgsSettingsTreeNodeData *>( obj );
}

void QgsSettingsTreeModel::updateSettingNodeAtIndex( const QModelIndex &index )
{
  QgsSettingsTreeNodeData *node = index2node( index );
  Q_ASSERT( node->type() == QgsSettingsTreeNodeData::Type::Setting );
  node->updateSettingNode();
  emit dataChanged( index.parent().siblingAtColumn( 0 ), index.parent().siblingAtColumn( columnCount( index.parent() ) ) );
}


QModelIndex QgsSettingsTreeModel::index( int row, int column, const QModelIndex &parent ) const
{
  if ( column < 0 || column >= columnCount( parent ) ||
       row < 0 || row >= rowCount( parent ) )
    return QModelIndex();

  QgsSettingsTreeNodeData *n = index2node( parent );
  if ( !n )
    return QModelIndex(); // have no children


  return createIndex( row, column, static_cast<QObject *>( n->children().at( row ) ) );
}

QModelIndex QgsSettingsTreeModel::parent( const QModelIndex &child ) const
{
  if ( !child.isValid() )
    return QModelIndex();

  if ( QgsSettingsTreeNodeData *n = index2node( child ) )
  {
    return indexOfParentSettingsTreeNode( n->parent() ); // must not be null
  }
  else
  {
    Q_ASSERT( false ); // no other node types!
    return QModelIndex();
  }
}

QModelIndex QgsSettingsTreeModel::indexOfParentSettingsTreeNode( QgsSettingsTreeNodeData *parentNode ) const
{
  Q_ASSERT( parentNode );

  const QgsSettingsTreeNodeData *grandParentNode = parentNode->parent();
  if ( !grandParentNode )
    return QModelIndex(); // root node -> invalid index

  int row = grandParentNode->children().indexOf( parentNode );
  Q_ASSERT( row >= 0 );

  return createIndex( row, 0, static_cast<QObject *>( parentNode ) );
}

int QgsSettingsTreeModel::rowCount( const QModelIndex &parent ) const
{
  QgsSettingsTreeNodeData *n = index2node( parent );
  if ( !n )
    return 0;

  return n->children().count();
}

int QgsSettingsTreeModel::columnCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent )
  return 3;
}

QVariant QgsSettingsTreeModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() || index.column() > columnCount( index ) )
    return QVariant();

  QgsSettingsTreeNodeData *node = index2node( index );

  switch ( static_cast<Column>( index.column() ) )
  {
    case Column::Name:
    {
      if ( role == Qt::DisplayRole || role == Qt::EditRole )
      {
        return node->name();
      }
      break;
    }

    case Column::Value:
    {
      if ( role == Qt::DisplayRole || role == Qt::EditRole )
      {
        return node->value();
      }
      if ( role == Qt::FontRole )
      {
        if ( !node->exists() )
        {
          QFont font;
          font.setItalic( true );
          return font;
        }
      }
      if ( role == Qt::BackgroundRole )
      {
        if ( node->type() == QgsSettingsTreeNodeData::Type::Setting )
        {
          switch ( node->setting()->settingsType() )
          {
            case Qgis::SettingsType::Custom:
            case Qgis::SettingsType::Variant:
            case Qgis::SettingsType::String:
            case Qgis::SettingsType::StringList:
            case Qgis::SettingsType::VariantMap:
            case Qgis::SettingsType::Bool:
            case Qgis::SettingsType::Integer:
            case Qgis::SettingsType::Double:
            case Qgis::SettingsType::EnumFlag:
              break;

            case Qgis::SettingsType::Color:
              return node->value();
          }
        }
      }
      break;
    }

    case Column::Description:
    {
      if ( node->type() == QgsSettingsTreeNodeData::Type::Setting )
      {
        if ( role == Qt::DisplayRole || role == Qt::EditRole )
        {
          return node->setting()->description();
        }
      }
      break;
    }

    default:
      break;
  }

  return QVariant();
}


QVariant QgsSettingsTreeModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
  if ( orientation == Qt::Orientation::Horizontal && role == Qt::DisplayRole )
  {
    switch ( static_cast<Column>( section ) )
    {
      case Column::Name:
        return tr( "Name" );
      case Column::Value:
        return tr( "Value" );
      case Column::Description:
        return tr( "Description" );
    };
  }

  return QVariant();
}



QgsSettingsTreeItemDelegate::QgsSettingsTreeItemDelegate( QgsSettingsTreeModel *model, QObject *parent )
  : QItemDelegate( parent )
  , mModel( model )
{
}

QWidget *QgsSettingsTreeItemDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
  Q_UNUSED( option )
  if ( static_cast<QgsSettingsTreeModel::Column>( index.column() ) == QgsSettingsTreeModel::Column::Value )
  {
    QgsSettingsTreeNodeData *nodeData = mModel->index2node( index );
    if ( nodeData->type() == QgsSettingsTreeNodeData::Type::Setting )
    {
      QWidget *widget = QgsGui::settingsEditorWidgetRegistry()->createEditor( nodeData->setting(), nodeData->namedParentNodes(), parent );
      return widget;
    }
  }
  return nullptr;
}

void QgsSettingsTreeItemDelegate::setEditorData( QWidget *editor, const QModelIndex &index ) const
{
  Q_UNUSED( index )
  QgsSettingsEditorWidgetWrapper *eww = QgsSettingsEditorWidgetWrapper::fromWidget( editor );
  eww->setWidgetFromSetting();
}

void QgsSettingsTreeItemDelegate::setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex &index ) const
{
  Q_UNUSED( index )
  Q_UNUSED( model )
  QgsSettingsEditorWidgetWrapper *eww = QgsSettingsEditorWidgetWrapper::fromWidget( editor );
  eww->setSettingFromWidget();
  mModel->updateSettingNodeAtIndex( index );
}


