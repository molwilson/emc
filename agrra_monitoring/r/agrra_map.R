library(htmlwidgets)
library(leaflet)
library(mapview)
library(webshot2)
library(dplyr)
library(htmltools)
library(sf)
library(ggplot2)
library(rnaturalearth)
library(rnaturalearthhires)
library(rnaturalearthdata)
library(ggspatial)
library(here)

save_path <- "/Users/margaretwilson/Github/emc/agrra_monitoring/figs"

# Regional map
world <- ne_countries(scale = "large", returnclass = "sf")
ggplot() +
  geom_sf(data = world, fill = "gray80", col = "gray80", size = .25) +
  annotate(geom = "text", x = -65, y = 26, label = "Atlantic Ocean", fontface = "italic", color = "grey80", size = 4) +
  annotate(geom = "text", x = -72.5, y = 16, label = "Caribbean Sea", fontface = "italic", color = "grey80", size = 4) +
  coord_sf(xlim = c(-86, -58), ylim = c(8, 36), expand = FALSE) +
  # annotation_north_arrow(
  #   location = "tr",
  #   which_north = "true",
  #   style = north_arrow_orienteering(
  #     text_size = 8,
  #     line_width = 0.8
  #   ),
  #   height = unit(1.2, "cm"),
  #   width = unit(1.2, "cm"),
  #   pad_x = unit(0.4, "cm"),
  #   pad_y = unit(0.4, "cm")
  # ) +
  theme_minimal() +
  theme(
    panel.background = element_rect(
      fill = "white",
      color = NA
    ),
    plot.background = element_rect(
      fill = "white",
      color = NA
    ),
    panel.grid.major = element_line(
      color = "grey85",
      linewidth = 0.3,
      linetype = "dotted"
    ),
    panel.grid.minor = element_blank(),
    axis.title = element_blank(),
    axis.text = element_text(size = 10),
    panel.border = element_rect(
      color = "black",
      fill = NA,
      linewidth = 0.5
    )) +
  labs(x = "", y = "")

ggsave("locator_map.png",
       width = 6,
       height = 6,
       dpi = 600)

# Antigua map

site_points <- sites_meta %>%
  distinct(code, latitude, longitude, site_cat, .keep_all = TRUE) %>%
  as.data.frame()

zone_ndnp <- st_read(here("agrra_monitoring", "data_raw", "WGS84_NDNP", "NDNP_WGS84.shp"))
zone_nemma <- st_read(here("agrra_monitoring", "data_raw", "WGS84_NEMMA", "NEMMA.shp"))

pal <- colorFactor(
  palette = c(
    "NEMMA" = "coral1",
    "NDNP"  = "goldenrod1"
  ),
  domain = site_points$site_cat
)

my_map <- leaflet(
  options = leafletOptions(
    preferCanvas = TRUE,
    zoomControl = FALSE,
    attributionControl = FALSE
  )
) %>%
  addProviderTiles("Esri.WorldImagery") %>%
  
  # add zone outlines
  addPolygons(
    data = zone_ndnp,
    
    color = "white",     # outline color
    weight = 2,          # line thickness
    opacity = 1,
    
    fill = FALSE         # no polygon fill
  ) %>%
  
  addPolygons(
    data = zone_nemma,
    
    color = "white",     # outline color
    weight = 2,          # line thickness
    opacity = 1,
    
    fill = FALSE         # no polygon fill
  ) %>%
  
  addCircleMarkers(
    data = site_points,
    lng = ~longitude,
    lat = ~latitude,
    
    radius = 2,
    
    # solid filled circles
    stroke = FALSE,
    fill = TRUE,
    
    # use palette function instead of ifelse
    color = ~pal(site_cat),
    fillColor = ~pal(site_cat),
    
    opacity = 1,
    fillOpacity = 1,
    
    label = NULL
  ) %>%
  
  addLabelOnlyMarkers(
    data = site_points,
    lng = ~longitude,
    lat = ~latitude,
    
    label = ~code,
    
    labelOptions = labelOptions(
      noHide = TRUE,
      direction = "top",
      textOnly = TRUE,
      
      style = list(
        "font-size" = "10px",
        "font-weight" = "bold",
        "color" = "white",
        "text-shadow" = "
        -1px -1px 0 #000,
         1px -1px 0 #000,
        -1px  1px 0 #000,
         1px  1px 0 #000
      "
      )
    )
  ) %>%
  
  # add scale bar
  addScaleBar(
    position = "bottomleft",
    options = scaleBarOptions(
      metric = TRUE,
      imperial = FALSE
    )
  ) %>%
  
  setView(
    lng = -61.8,
    lat = 17.08,
    zoom = 11.4
  )

my_map <- htmlwidgets::prependContent(
  my_map,
  htmltools::tags$style(
    htmltools::HTML("
      .leaflet-tile-pane img,
      .leaflet-tile {
        filter: grayscale(10%) brightness(140%) contrast(100%) !important;
        -webkit-filter: grayscale(10%) brightness(140%) contrast(100%) !important;
      }
    ")
  )
)

mapshot(
  my_map,
  file = file.path(save_path, "detail_map_labels.png"),
  selfcontained = FALSE,
  vwidth = 600,
  vheight = 600
)

# create a standalone 5 km scale bar
scale_bar <- ggplot() +
  coord_sf(
    xlim = c(0, 5000),   # 5000 meters = 5 km
    ylim = c(0, 1000),
    expand = FALSE
  ) +
  
  annotation_scale(
    location = "bl",
    width_hint = 1,      # use full width
    style = "bar",
    text_cex = 1,
    line_width = 0.6,
    line_col = "grey50",
    bar_cols = c("white", "grey50")
  ) +
  
  theme_void() +
  theme(
    panel.background = element_rect(
      fill = "transparent",
      color = NA
    ),
    plot.background = element_rect(
      fill = "transparent",
      color = NA
    )
  )
ggsave(
  "scale_bar_5km.png",
  scale_bar,
  bg = "transparent",
  width = 4,
  height = 1.2,
  dpi = 600
)
