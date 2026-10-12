# Offline map sources

`ui/WorldLand.js` contains outer land rings from Natural Earth's public-domain 1:110m land dataset, rounded to three decimal places. Source: https://github.com/nvkelso/natural-earth-vector/blob/master/geojson/ne_110m_land.geojson . Natural Earth license: https://www.naturalearthdata.com/about/terms-of-use/ . The equirectangular map is a geographic selection aid; it does not depict timezone boundaries.

`timezone-cities.json` contains timezone identifiers, representative city names, countries and ISO-6709 coordinates from IANA tzdata `zone.tab`, version 2026c. Source: https://data.iana.org/time-zones/tzdb/zone.tab . The source file states that it is in the public domain. Coordinates were converted to signed decimal degrees without moving locations. The app filters identifiers against the installed Qt timezone database. Clicking chooses the nearest listed representative city, not an address or a timezone polygon lookup; search remains available for exact city/zone selection.

No map tiles, geocoding requests or network connection are used at runtime.
