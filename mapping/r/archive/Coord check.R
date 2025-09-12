library(tidyverse)
library(here)
library(readxl)
library(janitor)
library(sf)
library(ggmap)
library(ggspatial)
library(ggrepel)

# Antigua shapefile
atg <- st_read(here("mapping", "shapefiles", "atg_adm_2019_shp", "atg_admbnda_adm1_2019.shp")) %>%
  st_union() %>%
  st_sf()

# data points
data_sites <- read_excel(here("mapping", "EMC Antigua_post bleaching acroporid surveys_2025.xlsx"), sheet = "acroporid survey") %>%
  clean_names()

# set lat/lon for graph (map) boundaries
lons = c(-62.0, -61.3)
lats = c(16.9, 17.8)

# basic map to check coordinates
quartz()
ggplot() +
  geom_sf(data = atg, fill = "slategray", color = "slategray") + # ATG basemap
  coord_sf(xlim = lons, ylim = lats, expand = FALSE) + # setting map boundaries
  geom_point(data = data_sites, 
             mapping = aes(x = longitude_dd, y = latitude_dd), 
             alpha = 0.7) + # add sites
  geom_text_repel(data = data_sites,
            mapping = aes(x = longitude_dd, y = latitude_dd, label = genotype_id),
            size = 1,
            max.overlaps = Inf) +
  theme_bw()



