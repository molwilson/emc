
library(sf)
library(here)

atg <- st_read(here("mapping", "shapefiles", "atg_adm_2019_shp", "atg_admbnda_adm1_2019.shp")) %>%
  st_union() %>%
  st_sf()

# Save as .rds for faster loading
saveRDS(atg, here("mapping", "r", "collection_map", "atg_union.rds"))
