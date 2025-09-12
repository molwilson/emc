library(tidyverse)
library(here)
library(readxl)
library(janitor)
library(sf)
library(ggplot2)
library(prettymapr)
library(ggspatial)
library(sp)

dat <- read_excel(here("mapping", "data", "Genotype Tracking.xlsx"), sheet = "collection") %>%
  clean_names() %>%
  filter(genotype != "DCYL02") # missing coordinates

sf_locs_main_db <- sf::st_as_sf(dat, coords = c("lon", "lat")) %>% 
  sf::st_set_crs(4326)

esri_ocean <- paste0('https://services.arcgisonline.com/arcgis/rest/services/',
                     'Ocean/World_Ocean_Base/MapServer/tile/${z}/${y}/${x}.jpeg')

x11()
ggplot() + 
  annotation_map_tile(type = esri_ocean, progress = "none") +
  # annotation_map_tile(type = "osm") +
  layer_spatial(sf_locs_main_db, aes(), color = "#F8766D", alpha = 0.6, linewidth = 0.75) +
  scale_linewidth(range = 1.4)+
  scale_x_continuous(expand = expand_scale(mult = c(.1, .15))) +
  scale_y_continuous(expand = expand_scale(mult = c(.1, .1))) +
  # scale_color_manual(values = Paired) +
  # scale_color_brewer(palette = "Paired") +  # Spectral
  theme(axis.text.x = element_text(size=15),axis.text.y=element_text(size=15))+
  ggtitle("Observed Clymene Dolphin Locations", subtitle = "Own data base")+
  theme(axis.title.x=element_text(size=24,face="plain"),axis.title.y=element_text(size=24,face="plain") ,
        legend.position = "none") + 
  theme(plot.title = element_text(size = 16))



library(raster)     # for raster data
library(ggplot2)    # plotting
library(sf)         # for spatial objects

# Example: download a GEBCO netCDF (here assume local file "gebco.tif")
# You can clip/download GEBCO tiles at https://www.gebco.net/

bathy <- raster("gebco_2023_n-20.0_s-25.0_w-65.0_e-60.0.tif") # example extent

# Convert raster to dataframe for ggplot
bathy_df <- as.data.frame(bathy, xy = TRUE)
names(bathy_df)[3] <- "depth"

# Plot with custom gradient (blue for shallow, dark navy for deep)
ggplot(bathy_df, aes(x, y, fill = depth)) +
  geom_raster() +
  scale_fill_gradientn(
    colours = c("#a6cee3", "#1f78b4", "#08306b"),
    values = scales::rescale(c(0, -100, -6000)), # shallow to deep
    na.value = "white"
  ) +
  coord_sf() +
  theme_minimal()

