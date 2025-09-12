# Merging NOAA datasets

library(tidyverse)
library(here) # for accessing files with directory
library(readxl) # for reading indv. sheets within excel files
library(janitor) # for cleaning variable names so they have consistent capitalization etc.library(sf)
library(sf)
library(ggplot2)


# merge coordinates

colonies_og <- read_excel(here("restoration_monitoring", "data_raw", "NOAA post-bleaching acroporid surveys.xlsx"), sheet = "acroporid survey") %>% 
  clean_names() %>%
  select(-c(latitude_dd, longitude_dd)) %>%
  filter(!complete_recent_mortality_yn %in% c("NA", "n/a"))

collection <- read_excel(here("restoration_monitoring", "data_raw", "Genotype tracking.xlsx"), sheet = "collection") %>% 
  clean_names() %>%
  filter(!lat %in% c("NA", "n/a")) %>%
  select(genotype_id = genotype, latitude_dd = lat, longitude_dd = lon) %>%
  mutate(across(c(latitude_dd, longitude_dd), as.numeric))

colonies <- left_join(colonies_og, collection, by = "genotype_id")

# check for remaining NAs

test <- colonies %>%
  filter(is.na(longitude_dd) | is.na(latitude_dd))

# plot coordinates to double check

atg <- st_read(here("mapping", "shapefiles", "atg_adm_2019_shp", "atg_admbnda_adm1_2019.shp")) %>%
  st_union() %>%
  st_sf()

lon_range <- range(colonies$longitude_dd)
lat_range <- range(colonies$latitude_dd)

ggplot() +
  geom_sf(data = atg, fill = "slategray", color = "slategray") +
  geom_point(
    data = colonies, 
    aes(x = longitude_dd, y = latitude_dd), 
    alpha = 0.7
  ) +
  coord_sf(
    xlim = lon_range,
    ylim = lat_range,
    expand = TRUE
  ) +
  theme_bw()

# export .csv

write_csv(colonies, here("restoration_monitoring", "data_outputs", "EMC Antigua post-bleaching surveys.csv"))
