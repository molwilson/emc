library(tidyverse)
library(janitor)
library(sf)
library(rnaturalearth)
library(rnaturalearthdata)
library(elevatr)
library(terra)
library(ggplot2)
library(ggspatial)
library(gganimate)
library(magick)


# Import data

shark_data <- read_excel(here("mapping", "data", "Shark Tagging Data.xlsx"), sheet = "Pings 260593PAT") %>% 
  rbind(read_excel(here("mapping", "data", "Shark Tagging Data.xlsx"), sheet = "Pings 283833PAT")) %>%
  clean_names() %>%
  slice(-c(3, 4, 8)) %>% # 3 points displayed as on land
  select(pttid, date = date_detected, lat = latitude_n, lon = longitude_w) %>%
  arrange(date)

# convert to sfs
shark_sf_4326 <- st_as_sf(shark_data, coords = c("lon", "lat"), crs = 4326)
shark_sf_3857 <- shark_sf_4326 %>% st_transform(3857) %>%
  cbind(st_coordinates(.)) %>%
  as.data.frame()

# extract data for plot
shark_df <- shark_sf_4326 %>%
  mutate(x = st_coordinates(.)[,1],
         y = st_coordinates(.)[,2]) %>%
  st_set_geometry(NULL)

# load shapefile
atg <- st_read(here("mapping", "shapefiles", "atg_adm_2019_shp", "atg_admbnda_adm1_2019.shp")) %>%
  st_union() %>%
  st_sf()

# animated plot with shapefile - indv. shark
p_anim <- ggplot() +
  geom_sf(data = atg, fill = "lightblue", color = "lightblue") +  # islands
  geom_path(data = shark_df %>% filter(pttid == "283833PAT"), 
            aes(x = x, y = y),
            color = "coral2", size = 1, alpha = 0.7, linetype = 2) +
  geom_point(data = shark_df%>% filter(pttid == "283833PAT"), 
             aes(x = x, y = y),
             color = "coral2", size = 2) +
  xlim(c(-62.15, -61.6)) + ylim(c(16.9, 17.3)) +
  labs(x = NULL, y = NULL, title = "Shark 283833 movement over time: {frame_along}") +
  theme_bw() +
  theme(panel.background = element_rect(fill = "steelblue"),
        panel.grid = element_blank()) +
  transition_reveal(date)

anim <- gganimate::animate(p_anim, nframes = 150, fps = 10, width = 600, height = 400, renderer = magick_renderer())
anim_save(here("mapping", "figs", "shark283833_track.gif"), animation = anim)

# animated plot with shapefile - mult sharks
p_anim <- ggplot() +
  geom_sf(data = atg, fill = "lightblue", color = "lightblue") +  # islands
  geom_path(data = shark_df, aes(x = x, y = y, group = pttid, color = pttid),
            size = 1, alpha = 0.7, linetype = 2) +
  geom_point(data = shark_df, aes(x = x, y = y, group = pttid, color = pttid),
             size = 2) +
  scale_color_manual(values = c("coral2", "gold")) +
  xlim(c(-62.15, -61.6)) + ylim(c(16.9, 17.3)) +
  labs(x = NULL, y = NULL, color = "ID", title = "Shark movement over time: {frame_along}") +
  theme_bw() +
  theme(panel.background = element_rect(fill = "steelblue"),
        panel.grid = element_blank()) +
  transition_reveal(date)

anim <- gganimate::animate(p_anim, nframes = 150, fps = 10, width = 600, height = 400, renderer = magick_renderer())
anim_save(here("mapping", "figs", "sharks_track.gif"), animation = anim)




# ESRI basemaps

esri_ocean <- paste0('https://services.arcgisonline.com/arcgis/rest/services/','Ocean/World_Ocean_Base/MapServer/tile/${z}/${y}/${x}.jpeg')
esri_sat <- "https://services.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/${z}/${y}/${x}.jpeg"
esri_world_imagery <- "https://services.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/${z}/${y}/${x}.jpeg"

# set range
lon_lim <- c(-62.22, -61.6)
lat_lim <- c(16.9, 17.3)
bbox_poly <- st_as_sfc(st_bbox(c(
  xmin = lon_lim[1],
  xmax = lon_lim[2],
  ymin = lat_lim[1],
  ymax = lat_lim[2]), 
  crs = 4326))
bbox_3857 <- st_transform(bbox_poly, 3857)
bb <- st_bbox(bbox_3857)
xlim_3857 <- c(bb["xmin"], bb["xmax"]) # for coord_sf()/ESRI
ylim_3857 <- c(bb["ymin"], bb["ymax"]) # for coord_sf()/ESRI

# still map

## buffered
ggplot() + 
  annotation_map_tile(type = esri_ocean, progress = "none") +
  layer_spatial(shark_sf, aes(), color = "#F8766D", alpha = 0.6, linewidth = 0.75) +
  scale_x_continuous(expand = expansion(mult = c(1, 1))) +
  scale_y_continuous(expand = expansion(mult = c(1, 1))) +
  theme_bw()

## manual limits
ggplot() +
  annotation_map_tile(type = esri_ocean, progress = "none") +
  layer_spatial(shark_sf_3857, aes(color = date), alpha = 0.6, linewidth = 0.75) +
  coord_sf(crs = st_crs(3857), xlim = xlim_3857, ylim = ylim_3857, expand = FALSE) +
  scale_color_viridis_c(option = "plasma") +
  theme_bw()

## attempting animation at high resolution

# static background (rendered once)
p_base <- ggplot() +
  annotation_map_tile(type = esri_world_imagery, progress = "none", zoomin = 0) +
  coord_sf(crs = st_crs(3857), xlim = xlim_3857, ylim = ylim_3857, expand = FALSE) +
  theme_bw(base_size = 18) +
  theme(
    panel.background = element_rect(fill = "steelblue"),
    panel.grid = element_blank(),
    legend.position = "bottom"
  ) 

# animate overlay
p_anim <- p_base +
  geom_path(data = shark_sf_3857, 
            aes(x = X, y = Y, group = pttid, color = pttid),
            linewidth = 1, alpha = 0.7, linetype = 2) +
  geom_point(data = shark_sf_3857,
             aes(x = X, y = Y, group = pttid, color = pttid),
             size = 2) +
  scale_color_manual(values = c("coral2", "gold")) +
  labs(x = NULL, y = NULL, color = "Shark ID",
       title = "Shark movement over time: {frame_along}") +
  transition_reveal(along = date)

# render animation
anim <- gganimate::animate(
  p_anim,
  nframes = 150, fps = 10,
  width = 1600, height = 1200,
  renderer = gifski_renderer())
anim_save(here("mapping", "figs", "sharks_track_esri_hr.gif"), animation = anim)




# Bathymetric data

# Load bathymetry raster (user-defined GeoTIFF)

## GEBCO: download manually from https://download.gebco.net/
gebco_sub <- rast(here("mapping", "rasters", "gebco_atg",
                       "gebco_2025_n17.8806_s16.6401_w-62.2738_e-61.2816.tif"))
gebco_df <- as.data.frame(gebco_sub, xy = TRUE)
colnames(gebco_df) <- c("x", "y", "depth")

## Sentinel2 2022 ATG-specific from TNC: downloaded from https://caribbeanscienceatlas.tnc.org/maps/28371a902f414fef830cc2e6ff9b3d8f
sentinel_sub <- rast(here("mapping", "rasters", "sentinel_atg",
                          "atg_bath_Sentinel2_2022_clip.tif"))
sentinel_df <- as.data.frame(sentinel_sub, xy = TRUE)
sentinel_df_small <- sentinel_df %>% sample_frac(0.5)
colnames(sentinel_df) <- c("x", "y", "depth")


p <- ggplot() +
  geom_raster(data = sentinel_df, aes(x = x, y = y, fill = depth)) +
  scale_fill_gradient2(low = "tan", mid = "lightblue", high = "steelblue4", midpoint = 0, name = "Depth (m)") +
  # geom_sf(data = shark_sf, color = "coral2", size = 1, alpha = 0.7) +
  geom_path(data = shark_df,
            aes(x = x, y = y),
            color = "coral2", size = 1, alpha = 0.7, linetype = 2) +
  geom_point(data = shark_df,
             aes(x = x, y = y),
             color = "coral2", size = 2) +
  xlim(c(-62.2, -61.6)) + ylim(c(16.9, 17.3)) +
  labs(x = NULL, y = NULL, title = "Shark 260593 movement over time: {frame_along}") +
  # transition_reveal(date) +
  coord_fixed() +
  theme_bw() +
  theme(panel.background = element_rect(fill = "steelblue4"),
        panel.grid = element_blank())

anim <- gganimate::animate(p, nframes = 100, fps = 5, width = 800, height = 600)
anim_save(here("mapping", "figs", "shark260593_track_bathy.gif"), animation = anim)

# split for speed?

# static background (rendered once)
p_base <- ggplot() +
  geom_raster(data = sentinel_df, aes(x = x, y = y, fill = depth)) +
  scale_fill_gradient2(low = "tan", mid = "lightblue", high = "steelblue4", midpoint = 0, name = "Depth (m)") +
  coord_fixed(xlim = c(-62.15, -61.6), ylim = c(16.9, 17.3)) +
  theme_bw() +
  theme(panel.background = element_rect(fill = "steelblue4"),
        panel.grid = element_blank())

# animate overlay
p_anim <- p_base +
  geom_path(data = shark_df, aes(x = x, y = y),
            color = "coral2", size = 1, alpha = 0.7, linetype = 2) +
  geom_point(data = shark_df, aes(x = x, y = y),
             color = "coral2", size = 2) +
  labs(x = NULL, y = NULL, title = "Shark movement over time: {frame_along}") +
  transition_reveal(date)

gganimate::animate(p_anim, nframes = 150, fps = 10)
anim_save(here("mapping", "figs", "shark260593_track_bathy.gif"), animation = anim)














