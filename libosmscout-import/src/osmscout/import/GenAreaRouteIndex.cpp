/*
  This source is part of the libosmscout library
  Copyright (C) 2020  Lukas Karas

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/import/GenAreaRouteIndex.h>

#include <osmscout/RouteDataFile.h>
#include <osmscout/AreaRouteIndex.h>

namespace osmscout {

  AreaRouteIndexGenerator::AreaRouteIndexGenerator():
    AreaIndexGenerator<Route>("route",
                              "rotues",
                              RouteDataFile::ROUTE_DAT,
                              AreaRouteIndex::AREA_ROUTE_IDX)
  {}

  void AreaRouteIndexGenerator::GetDescription(const ImportParameter& /*parameter*/,
                                               ImportModuleDescription& description) const
  {
    description.SetName("AreaRouteIndexGenerator");
    description.SetDescription("Index routes for area lookup");

    description.AddRequiredFile(RouteDataFile::ROUTE_DAT);

    description.AddProvidedFile(AreaRouteIndex::AREA_ROUTE_IDX);
  }

  void AreaRouteIndexGenerator::WriteTypeId(const TypeConfigRef& typeConfig,
                                            const TypeInfoRef &type,
                                            FileWriter &writer) const
  {
    writer.WriteTypeId(type->GetRouteId(),
                       typeConfig->GetRouteTypeIdBytes());
  }

  bool AreaRouteIndexGenerator::Import(const TypeConfigRef& typeConfig,
                                       const ImportParameter& parameter,
                                       Progress& progress)
  {
    return MakeAreaIndex(typeConfig,
                         parameter,
                         progress,
                         typeConfig->GetRouteTypes(),
                         // TODO: use route specific parameters
                         parameter.GetAreaWayMinMag(),
                         parameter.GetAreaWayIndexMaxLevel(),
                         parameter.GetWayDataMemoryMaped());
  }

}
