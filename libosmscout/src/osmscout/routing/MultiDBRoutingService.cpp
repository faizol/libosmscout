/*
  This source is part of the libosmscout library
  Copyright (C) 2016  Tim Teulings
  Copyright (C) 2017  Lukas Karas

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

#include <algorithm>

#include <osmscout/routing/RoutingProfile.h>
#include <osmscout/routing/RoutingService.h>
#include <osmscout/routing/SimpleRoutingService.h>
#include <osmscout/routing/MultiDBRoutingService.h>
#include <osmscout/Pixel.h>

#include <osmscout/system/Assert.h>

#include <osmscout/util/Geometry.h>
#include <osmscout/util/Logger.h>
#include <osmscout/util/StopClock.h>

//#define DEBUG_ROUTING

#include <iomanip>
#include <iostream>

namespace osmscout {

  RouterDBFiles::RouterDBFiles():
     routeNodeDataFile(RoutingService::GetDataFilename(osmscout::RoutingService::DEFAULT_FILENAME_BASE),
                       RoutingService::GetIndexFilename(osmscout::RoutingService::DEFAULT_FILENAME_BASE),
                       /*indexCacheSize*/ 12000,
                       /*dataCacheSize*/ 1000),
     junctionDataFile(RoutingService::FILENAME_INTERSECTIONS_DAT,
                      RoutingService::FILENAME_INTERSECTIONS_IDX,
                      /*indexCacheSize*/ 10000,
                      /*dataCacheSize*/ 1000)
  {
  }

  RouterDBFiles::~RouterDBFiles()
  {
  }

  bool RouterDBFiles::Open(DatabaseRef database)
  {
    if (!objectVariantDataFile.Load(*(database->GetTypeConfig()),
                                    AppendFileToDir(database->GetPath(),
                                                    RoutingService::GetData2Filename(osmscout::RoutingService::DEFAULT_FILENAME_BASE)))){
      return false;
    }
    if (!routeNodeDataFile.Open(database->GetTypeConfig(),
                                database->GetPath(),
                                true,
                                database->GetRouterDataMMap())) {
      log.Error() << "Cannot open '" <<  database->GetPath() << "'!";
      return false;
    }

    if (!junctionDataFile.Open(database->GetTypeConfig(),
                               database->GetPath(),
                               false,
                               false)) {
      return false;
    }

    return true;
  }

  void RouterDBFiles::Close()
  {
    routeNodeDataFile.Close();
    junctionDataFile.Close();
  }

  MultiDBRoutingService::MultiDBRoutingService(const RouterParameter& parameter,
                                               std::vector<DatabaseRef> databases):
    AbstractRoutingService<MultiDBRoutingState>(parameter),
    isOpen(false)
  {
    DatabaseId id=0;
    for (auto &db:databases){
      this->databaseMap[db->GetPath()]=id;
      this->databases[id]=db;
      id++;
    }
  }

  MultiDBRoutingService::~MultiDBRoutingService()
  {
    Close();
  }

  bool MultiDBRoutingService::Open(RoutingProfileBuilder profileBuilder)
  {
    if (databases.empty()){
      return false;
    }
    RouterParameter routerParameter;
    routerParameter.SetDebugPerformance(debugPerformance);

    isOpen=true;
    for (auto &entry:databases){
      DatabaseRef database=entry.second;
      SimpleRoutingServiceRef router=std::make_shared<osmscout::SimpleRoutingService>(
                                              entry.second,
                                              routerParameter,
                                              osmscout::RoutingService::DEFAULT_FILENAME_BASE);
      if (!router->Open()){
        Close();
        return false;
      }
      services[entry.first]=router;
      profiles[entry.first]=profileBuilder(database);

      RouterDBFilesRef dataFile=std::make_shared<RouterDBFiles>();
      if (!dataFile->Open(database)) {
        Close();
        return false;
      }
      routerFiles[entry.first]=dataFile;

    }
    return true;
  }

  void MultiDBRoutingService::Close(){
    if (!isOpen){
      return;
    }
    for (auto &entry:services){
      entry.second->Close();
    }
    for (auto entry:routerFiles){
      entry.second->Close();
    }
    services.clear();
    profiles.clear();
    isOpen=false;
  }

  RoutePosition MultiDBRoutingService::GetClosestRoutableNode(const GeoCoord& coord,
                                                              double radius,
                                                              std::string databasePathHint) const
  {
    RoutePosition position;

    // first try to find in hinted database
    auto databaseHint=databaseMap.find(databasePathHint);
    if (databaseHint!=databaseMap.end()){
      auto it=services.find(databaseHint->second);
      if (it!=services.end()){
        position=it->second->GetClosestRoutableNode(coord,*(profiles.at(it->first)),radius);
        if (position.IsValid()){
          return RoutePosition(position.GetObjectFileRef(),
                               position.GetNodeIndex(),
                               /*database*/ it->first);
        }
      }
    }

    // try all other databases
    for (auto &entry:services){
      if (databaseHint!=databaseMap.end() && entry.first==databaseHint->second){
        continue;
      }

      position=entry.second->GetClosestRoutableNode(coord,*(profiles.at(entry.first)),radius);
      if (position.IsValid()){
          return RoutePosition(position.GetObjectFileRef(),
                               position.GetNodeIndex(),
                               /*database*/ entry.first);
      }
    }
    return position;
  }

  const double MultiDBRoutingService::CELL_MAGNIFICATION=std::pow(2,16);
  const double MultiDBRoutingService::LAT_CELL_FACTOR=180.0/ MultiDBRoutingService::CELL_MAGNIFICATION;
  const double MultiDBRoutingService::LON_CELL_FACTOR=360.0/ MultiDBRoutingService::CELL_MAGNIFICATION;

  Pixel MultiDBRoutingService::GetCell(const osmscout::GeoCoord& coord)
  {
    return osmscout::Pixel(uint32_t((coord.GetLon()+180.0)/ LON_CELL_FACTOR),
                           uint32_t((coord.GetLat()+90.0)/ LAT_CELL_FACTOR));
  }

  Vehicle MultiDBRoutingService::GetVehicle(const MultiDBRoutingState& state)
  {
    return state.GetVehicle();
  }
  bool MultiDBRoutingService::CanUseForward(const MultiDBRoutingState& state,
                                            const DatabaseId& database,
                                            const WayRef& way)
  {
    return state.GetProfile(database)->CanUseForward(*way);
  }

  bool MultiDBRoutingService::CanUseBackward(const MultiDBRoutingState& state,
                                             const DatabaseId& database,
                                             const WayRef& way)
  {
    return state.GetProfile(database)->CanUseBackward(*way);
  }

  double MultiDBRoutingService::GetCosts(const MultiDBRoutingState& state,
                                         const DatabaseId database,
                                         const RouteNode& routeNode,
                                         size_t pathIndex)
  {
    ;
    return state.GetProfile(database)->GetCosts(routeNode,
                                                routerFiles[database]->objectVariantDataFile.GetData(),
                                                pathIndex);
  }

  double MultiDBRoutingService::GetCosts(const MultiDBRoutingState& state,
                                         const DatabaseId database,
                                         const WayRef &way,
                                         double wayLength)
  {
    return state.GetProfile(database)->GetCosts(*way,wayLength);
  }

  double MultiDBRoutingService::GetEstimateCosts(const MultiDBRoutingState& state,
                                                 const DatabaseId database,
                                                 double targetDistance)
  {
    return state.GetProfile(database)->GetCosts(targetDistance);
  }

  double MultiDBRoutingService::GetCostLimit(const MultiDBRoutingState& state,
                              const DatabaseId database,
                              double targetDistance)
  {
    RoutingProfileRef profile=state.GetProfile(database);
    return profile->GetCosts(profile->GetCostLimitDistance())+targetDistance*profile->GetCostLimitFactor();
  }

  bool MultiDBRoutingService::ReadCellsForRoutingTree(osmscout::Database& database,
                                                      std::unordered_set<uint64_t>& cells)
  {
    std::string filename=std::string(osmscout::RoutingService::DEFAULT_FILENAME_BASE)+".dat";
    std::string fullFilename=osmscout::AppendFileToDir(database.GetPath(),filename);

    osmscout::FileScanner scanner;

    try {
      uint32_t count;

      std::cout << "Opening routing file '" << fullFilename << "'" << std::endl;

      scanner.Open(fullFilename,osmscout::FileScanner::Sequential,false);

      scanner.Read(count);

      for (uint32_t i=1; i<=count; i++) {
        osmscout::RouteNode node;
        osmscout::Pixel     cell;

        node.Read(*database.GetTypeConfig(),scanner);

        cell=GetCell(node.GetCoord());

        cells.insert(cell.GetId());

        //std::cout << node.GetCoord().GetDisplayText() << " " << cell.GetId() << std::endl;
      }

      scanner.Close();
    }
    catch (osmscout::IOException& e) {
      std::cerr << "Error while reading '" << fullFilename << "': " << e.GetDescription() << std::endl;
      return false;
    }

    return true;
  }

  bool MultiDBRoutingService::ReadRouteNodesForCells(osmscout::Database& database,
                                                     std::unordered_set<uint64_t>& cells,
                                                     std::unordered_set<Id>& routeNodes)
  {
    std::string filename=std::string(osmscout::RoutingService::DEFAULT_FILENAME_BASE)+".dat";
    std::string fullFilename=osmscout::AppendFileToDir(database.GetPath(),filename);

    osmscout::FileScanner scanner;

    try {
      uint32_t count;

      std::cout << "Opening routing file '" << fullFilename << "'" << std::endl;

      scanner.Open(fullFilename,osmscout::FileScanner::Sequential,false);

      scanner.Read(count);

      for (uint32_t i=1; i<=count; i++) {
        osmscout::RouteNode node;
        osmscout::Pixel     cell;

        node.Read(*database.GetTypeConfig(),scanner);

        cell=GetCell(node.GetCoord());

        if (cells.find(cell.GetId())!=cells.end()) {
          routeNodes.insert(node.GetId());
        }
      }

      scanner.Close();
    }
    catch (osmscout::IOException& e) {
      std::cerr << "Error while reading '" << fullFilename << "': " << e.GetDescription() << std::endl;
      return false;
    }

    return true;
  }

  bool MultiDBRoutingService::FindCommonRoutingNodes(DatabaseRef &database1,
                                                     DatabaseRef &database2,
                                                     std::set<Id> &commonRouteNodes)
  {

    std::unordered_set<uint64_t> cells1;
    std::unordered_set<uint64_t> cells2;

    std::cout << "Reading route node cells of database 1..." << std::endl;

    if (!ReadCellsForRoutingTree(*database1,
                                 cells1)){
      return false;
    }

    std::cout << "Found route nodes in " << cells1.size() << " cells" << std::endl;

    std::cout << "Reading route node cells of database 2..." << std::endl;

    if (!ReadCellsForRoutingTree(*database2,
                                 cells2)){
      return false;
    }

    std::cout << "Found route nodes in " << cells2.size() << " cells" << std::endl;

    std::cout << "Detecting common cells..." << std::endl;

    std::unordered_set<uint64_t> commonCells;

    for (const auto cell : cells1) {
      if (cells2.find(cell)!=cells2.end()) {
        commonCells.insert(cell);
      }
    }

    cells1.clear();
    cells2.clear();

    std::cout << "There are " << commonCells.size() << " common cells" << std::endl;

    std::unordered_set<Id> routeNodes1;
    std::unordered_set<Id> routeNodes2;

    std::cout << "Reading route nodes in common cells of database 1..." << std::endl;

    if (!ReadRouteNodesForCells(*database1,
                                commonCells,
                                routeNodes1)){
      return false;
    }

    std::cout << "Found " << routeNodes1.size() << " route nodes in common cells" << std::endl;

    std::cout << "Reading route nodes in common cells of database 2..." << std::endl;

    if (!ReadRouteNodesForCells(*database2,
                                commonCells,
                                routeNodes2)){
      return false;
    }

    std::cout << "Found " << routeNodes2.size() << " route nodes in common cells" << std::endl;

    std::cout << "Detecting common route nodes..." << std::endl;


    for (const auto routeNode : routeNodes1) {
      if (routeNodes2.find(routeNode)!=routeNodes2.end()) {
        commonRouteNodes.insert(routeNode);
      }
    }

    routeNodes1.clear();
    routeNodes2.clear();

    std::cout << "There are " << commonRouteNodes.size() << " common route nodes" << std::endl;
    return true;
  }

  bool MultiDBRoutingService::ReadRouteNodeEntries(DatabaseRef &database,
                                            std::set<Id> &commonRouteNodes,
                                            std::unordered_map<Id,osmscout::RouteNodeRef> &routeNodeEntries)
  {
    osmscout::IndexedDataFile<Id,osmscout::RouteNode> routeNodeFile(std::string(osmscout::RoutingService::DEFAULT_FILENAME_BASE)+".dat",
                                                                    std::string(osmscout::RoutingService::DEFAULT_FILENAME_BASE)+".idx",
                                                                    /*indexCacheSize*/12000,
                                                                    /*dataCacheSize*/1000);

    std::cout << "Opening routing database..." << std::endl;

    if (!routeNodeFile.Open(database->GetTypeConfig(),
                            database->GetPath(),
                            true,
                            true)) {
      std::cerr << "Cannot open routing database" << std::endl;
      return false;
    }



    if (!routeNodeFile.Get(commonRouteNodes,
                           routeNodeEntries)) {
      std::cerr << "Cannot retrieve route nodes" << std::endl;
      routeNodeFile.Close();
      return false;
    }
    routeNodeFile.Close();
    return true;
  }

  bool MultiDBRoutingService::CanUse(const MultiDBRoutingState& /*state*/,
                                     const DatabaseId database,
                                     const RouteNode& routeNode,
                                     size_t pathIndex)
  {

    RoutingProfileRef profile=profiles[database];
    RouterDBFilesRef dataFiles=routerFiles[database];
    return profile->CanUse(routeNode,dataFiles->objectVariantDataFile.GetData(),pathIndex);
  }

  bool MultiDBRoutingService::GetRouteNodesByOffset(const std::set<DBFileOffset> &routeNodeOffsets,
                                                    std::unordered_map<DBFileOffset,RouteNodeRef> &routeNodeMap)
  {
    std::unordered_map<DatabaseId,std::set<FileOffset>> offsetMap;
    for (const auto &offset:routeNodeOffsets){
      offsetMap[offset.database].insert(offset.offset);
    }
    for (const auto &entry:offsetMap){
      std::vector<RouteNodeRef> nodes;
      if (!routerFiles[entry.first]->routeNodeDataFile.Get(entry.second,nodes)){
        return false;
      }
      for (const auto &node:nodes){
        routeNodeMap[DBFileOffset(entry.first,node->GetFileOffset())]=node;
      }
    }
    return true;
  }

  bool MultiDBRoutingService::GetRouteNodeByOffset(const DBFileOffset &offset,
                                                   RouteNodeRef &node)
  {
    RouterDBFilesRef dataFiles=routerFiles[offset.database];
    return dataFiles->routeNodeDataFile.GetByOffset(offset.offset, node);
  }

  bool MultiDBRoutingService::GetRouteNodeOffset(const DatabaseId &database,
                                                 const Id &id,
                                                 FileOffset &offset)
  {
    return routerFiles[database]->routeNodeDataFile.GetOffset(id,offset);
  }

  bool MultiDBRoutingService::GetWayByOffset(const DBFileOffset &offset,
                                             WayRef &way)
  {    
    return databases[offset.database]->GetWayByOffset(offset.offset,way);
  }

  bool MultiDBRoutingService::GetWaysByOffset(const std::set<DBFileOffset> &wayOffsets,
                                              std::unordered_map<DBFileOffset,WayRef> &wayMap)
  {
    std::unordered_map<DatabaseId,std::set<FileOffset>> offsetMap;
    for (const auto &offset:wayOffsets){
      offsetMap[offset.database].insert(offset.offset);
    }
    for (const auto &entry:offsetMap){
      std::vector<WayRef> ways;
      if (!databases[entry.first]->GetWaysByOffset(entry.second,ways)){
        return false;
      }
      for (const auto &way:ways){
        wayMap[DBFileOffset(entry.first,way->GetFileOffset())]=way;
      }
    }
    return true;
  }

  bool MultiDBRoutingService::GetAreasByOffset(const std::set<DBFileOffset> &areaOffsets,
                                               std::unordered_map<DBFileOffset,AreaRef> &areaMap)
  {
    std::unordered_map<DatabaseId,std::set<FileOffset>> offsetMap;
    for (const auto &offset:areaOffsets){
      offsetMap[offset.database].insert(offset.offset);
    }
    for (const auto &entry:offsetMap){
      std::vector<AreaRef> areas;
      if (!databases[entry.first]->GetAreasByOffset(entry.second,areas)){
        return false;
      }
      for (const auto &area:areas){
        areaMap[DBFileOffset(entry.first,area->GetFileOffset())]=area;
      }
    }
    return true;
  }

  bool MultiDBRoutingService::ResolveRouteDataJunctions(RouteData& route)
  {
    for (auto &filesEntry:routerFiles){
      DatabaseId dbId=filesEntry.first;
      std::set<Id> nodeIds;

      for (const auto& routeEntry : route.Entries()) {
        if (routeEntry.GetCurrentNodeId()!=0 && routeEntry.GetDatabaseId()==dbId) {
          nodeIds.insert(routeEntry.GetCurrentNodeId());
        }
      }

      std::vector<JunctionRef> junctions;

      if (!filesEntry.second->junctionDataFile.Get(nodeIds,
                                                  junctions)) {
        log.Error() << "Error while resolving junction ids to junctions";
        return false;
      }

      nodeIds.clear();

      std::unordered_map<Id,JunctionRef> junctionMap;

      for (const auto& junction : junctions) {
        junctionMap.insert(std::make_pair(junction->GetId(),junction));
      }

      junctions.clear();

      for (auto& routeEntry : route.Entries()) {
        if (routeEntry.GetCurrentNodeId()!=0 && routeEntry.GetDatabaseId()==dbId) {
          auto junctionEntry=junctionMap.find(routeEntry.GetCurrentNodeId());
          if (junctionEntry!=junctionMap.end()) {
            routeEntry.SetObjects(junctionEntry->second->GetObjects());
          }
        }
      }
    }

    return true;
  }

  bool MultiDBRoutingService::GetRouteNode(const DatabaseId &database,
                                           const Id &id,
                                           RouteNodeRef &node)
  {
    RouterDBFilesRef dataFiles=routerFiles[database];
    return dataFiles->routeNodeDataFile.Get(id, node);
  }

  RoutingResult MultiDBRoutingService::CalculateRoute(const RoutePosition &start,
                                                      const RoutePosition &target,
                                                      const RoutingParameter &parameter)
  {
    RoutingResult result;

    if (start.GetDatabaseId()==target.GetDatabaseId()){
      auto it=services.find(start.GetDatabaseId());
      if (it==services.end()){
        return result;
      }
      SimpleRoutingServiceRef service=it->second;
      return service->CalculateRoute(*(profiles.at(it->first)),start,target,parameter);
    }

    // start and target databases are different, try to find common route nodes
    auto it=databases.find(start.GetDatabaseId());
    if (it==databases.end()){
      std::cerr << "Can't find start database " << start.GetDatabaseId() << std::endl;
      return result;
    }
    DatabaseRef database1=it->second;

    it=databases.find(target.GetDatabaseId());
    if (it==databases.end()){
      std::cerr << "Can't find target database " << target.GetDatabaseId() << std::endl;
      return result;
    }
    DatabaseRef database2=it->second;

    std::set<Id> commonRouteNodes;
    if (!FindCommonRoutingNodes(database1,database2,commonRouteNodes)){
      std::cerr << "Can't find common routing nodes for databases " <<
        database1->GetPath() << ", " <<
        database2->GetPath() << std::endl;
      return result;
    }
    if (commonRouteNodes.empty()){
      std::cerr << "Can't find common routing nodes for databases " <<
        database1->GetPath() << ", " <<
        database2->GetPath() << std::endl;
      return result;
    }

    std::unordered_map<Id,osmscout::RouteNodeRef> routeNodeEntries1;
    if (!ReadRouteNodeEntries(database1,commonRouteNodes,routeNodeEntries1)){
      return result;
    }
    std::unordered_map<Id,osmscout::RouteNodeRef> routeNodeEntries2;
    if (!ReadRouteNodeEntries(database2,commonRouteNodes,routeNodeEntries2)){
      return result;
    }

    for (const auto& routeNodeEntry : routeNodeEntries1) {
      std::cout << routeNodeEntry.second->GetId() << " " << routeNodeEntry.second->GetCoord().GetDisplayText() << std::endl;
    }

    MultiDBRoutingState state(start.GetDatabaseId(),
                              target.GetDatabaseId(),
                              profiles[start.GetDatabaseId()],
                              profiles[target.GetDatabaseId()]);
    
    return AbstractRoutingService<MultiDBRoutingState>::CalculateRoute(state,
                                                                       start,
                                                                       target,
                                                                       parameter);
  }
}
