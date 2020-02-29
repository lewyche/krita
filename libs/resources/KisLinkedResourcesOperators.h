/*
 *  Copyright (c) 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef KISLINKEDRESOURCESOPERATORS_H
#define KISLINKEDRESOURCESOPERATORS_H

#include "kritaresources_export.h"

#include <KisResourcesInterface.h>
#include "kis_assert.h"

namespace KisLinkedResourcesOperators
{

namespace detail {
bool KRITARESOURCES_EXPORT isLocalResourcesStorage(KisResourcesInterfaceSP resourcesInterface);
void KRITARESOURCES_EXPORT assertInGuiThread();
KisResourcesInterfaceSP KRITARESOURCES_EXPORT createLocalResourcesStorage(const QList<KoResourceSP> &resources);
}

template <typename T>
struct ResourceTraits
{
};

/**
 * @return true if the configuration has all the necessary resources in
 * local storage. It mean it can be used in a threaded environment.
 *
 * @see createLocalResourcesSnapshot()
 */
template <typename T>
bool hasLocalResourcesSnapshot(const T *object)
{
    return detail::isLocalResourcesStorage(object->resourcesInterface());
}

/**
 * Loads all the linked resources either from the current resource interface
 * or from the embedded data. The object first tries to fetch the linked
 * resource from the current source, and only if it fails, tries to load
 * it from the embedded data.
 *
 * @param globalResourcesInterface if \p globalResourcesInterface is not null,
 * the resources are fetched from there, not from the internally stored resources
 * interface
 */
template <typename T>
void createLocalResourcesSnapshot(T *object, KisResourcesInterfaceSP globalResourcesInterface = nullptr)
{
    detail::assertInGuiThread();
    QList<KoResourceSP> resources =
        object->linkedResources(globalResourcesInterface ?
                                    globalResourcesInterface :
                                    object->resourcesInterface());
    object->setResourcesInterface(detail::createLocalResourcesStorage(resources));
}

/**
 * @brief creates an exact copy of the object and loads all the linked
 *        resources into the local storage.
 * @param globalResourcesInterface is an optional override for the
 *        resources interface used for fetching linked resources. If
 *        \p globalResourcesInterface is null, then object->resourcesInterface()
 *        is used.
 *
 * If a filter configuration object already has a resources snapshot, then
 * the function just clones the object without reloading anything.
 */
template <typename T, typename TypeSP = typename ResourceTraits<T>::template SharedPointerType<T>>
TypeSP cloneWithResourcesSnapshot(const T* object,
                                  KisResourcesInterfaceSP globalResourcesInterface = nullptr)
{
    auto clonedStorage = object->clone();
    TypeSP cloned = ResourceTraits<T>::template dynamicCastSP<T>(clonedStorage);

    if (!hasLocalResourcesSnapshot(cloned.data())) {
        createLocalResourcesSnapshot(cloned.data(), globalResourcesInterface);
        KIS_SAFE_ASSERT_RECOVER_NOOP(hasLocalResourcesSnapshot(cloned.data()));
    }

    return cloned;
}

}

#endif // KISLINKEDRESOURCESOPERATORS_H