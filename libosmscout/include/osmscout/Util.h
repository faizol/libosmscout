#ifndef OSMSCOUT_UTIL_H
#define OSMSCOUT_UTIL_H

/*
  Import/TravelJinni - Openstreetmap offline viewer
  Copyright (C) 2009  Tim Teulings

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

#include <stdint.h>

#include <sys/time.h>

#include <set>
#include <string>

class NumberSet
{
  typedef unsigned long Number;

  struct Data
  {
    virtual ~Data();
  };

  struct Refs : public Data
  {
    Data* refs[256];

    Refs();
    ~Refs();
  };

  struct Leaf : public Data
  {
    unsigned char values[32];

    Leaf();
  };

private:
  Refs refs;

public:
  NumberSet();
  ~NumberSet();
  void Insert(Number value);
  bool IsSet(Number value) const;
};

class StopClock
{
private:
  timeval start;
  timeval stop;

public:
  StopClock();

  void Stop();

  friend std::ostream& operator<<(std::ostream& stream, const StopClock& clock);
};

extern std::ostream& operator<<(std::ostream& stream, const StopClock& clock);

extern void GetKeysForName(const std::string& name, std::set<uint32_t>& keys);

extern bool EncodeNumber(unsigned long number,
                         size_t bufferLength,
                         char* buffer,
                         size_t& bytes);
extern bool DecodeNumber(const char* buffer, unsigned long& number, size_t& bytes);

extern bool GetFileSize(const std::string& filename, long& size);

double GetSphericalDistance(double aLon, double aLat,
                            double bLon, double bLat);
double GetEllipsoidalDistance(double aLon, double aLat,
                              double bLon, double bLat);



#endif
