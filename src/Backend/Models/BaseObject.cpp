/**
 * @file baseobject.cpp
 * @brief Implementation of the BaseObject class.
 * @author Ahmed Aredah
 * @date 2025-03-24
 */

#include "BaseObject.h"

 BaseObject::BaseObject(QObject* parent)
     : QObject(parent)
     , m_uniqueId(QUuid::createUuid().toString(QUuid::WithoutBraces))
 {
 }
 
 BaseObject::~BaseObject()
 {
     // Virtual destructor implementation
 }

 QString BaseObject::getInternalUniqueID() const
 {
     return m_uniqueId;
 }
