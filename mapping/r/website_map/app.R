library(tidyverse)
library(shiny)
library(leaflet)
library(readxl)
library(janitor)
library(lubridate)
library(sf)

################# set wd outside of app code #################
# setwd("mapping/r/website_map")
# setwd("~/Github/emc/mapping/r/website_map")

################################## data ##################################

collection_dat <- read_excel("Genotype tracking.xlsx", sheet = "collection") %>%
  clean_names() %>%
  filter(!lat %in% c("NA", "n/a")) %>%
  mutate(date = as.Date(as.numeric(date), origin = "1899-12-30"),
         year = year(ymd(date))
     ) %>%
  mutate(across(c(lat, lon), as.numeric),
         category = "collection") %>%
  select(name = genotype, category, latitude = lat, longitude = lon, year) # year

site_dat <- read_excel("Genotype tracking.xlsx", sheet = "site locations") %>%
  clean_names() %>%
  bind_rows(collection_dat) %>%
  mutate(category = str_to_title(category))
  

################################## app ##################################

ui <- fluidPage(
  titlePanel("AnuBlue Coral Restoration"),
  
  sidebarLayout(
    sidebarPanel(
      checkboxGroupInput(
        inputId = "category",
        label = "Select Site Type:",
        choices = unique(site_dat$category),
        selected = unique(site_dat$category)
      ),
      
      # Animated year slider
      sliderInput(
        inputId = "year",
        label = "Select Year:",
        min = min(site_dat$year, na.rm = TRUE),
        max = max(site_dat$year, na.rm = TRUE),
        value = max(site_dat$year, na.rm = TRUE),
        step = 1,
        sep = "",
        animate = animationOptions(
          interval = 1000,   # 1 second per step
          loop = FALSE,
          playButton = icon("play"),
          pauseButton = icon("pause")
        )
      )
    ),
    
    mainPanel(
      leafletOutput("map", height = 600)
    )
  )
)

server <- function(input, output, session) {
  
  # Define categories and palette
  categories <- c("nursery", "restoration site", "collection")
  pal <- colorFactor(
    palette = c("#FEE100", "#FF284B", "#FF7E5A"), # yellow, red, orange
    domain = categories
  )
  
  # Reactive filter: category + cumulative year
  filtered_data <- reactive({
    site_dat %>%
      filter(
        category %in% input$category,
        year <= input$year  # cumulative points up to selected year
      )
  })
  
  # Initial map
  output$map <- renderLeaflet({
    leaflet(options = leafletOptions(preferCanvas = TRUE)) %>%
      addProviderTiles("Esri.WorldImagery") %>%
      setView(
        lng = -61.8,
        lat = 17.08,
        zoom = 11.4
      )
  })
  
  # Dynamic markers
  observe({
    fd <- filtered_data()
    
    leafletProxy("map") %>%
      clearMarkers() %>%
      
      # Collection sites (small dots underneath)
      addCircleMarkers(
        data = fd %>% filter(category == "collection"),
        lng = ~longitude,
        lat = ~latitude,
        radius = 3,
        stroke = FALSE,
        fillColor = ~pal(category),
        fillOpacity = 0.7,
        popup = ~paste0("<strong>", name, "</strong><br>", category)
      ) %>%
      
      # Nursery + Restoration sites (larger circles on top)
      addCircleMarkers(
        data = fd %>% filter(category != "collection"),
        lng = ~longitude,
        lat = ~latitude,
        radius = 7,
        stroke = TRUE,
        weight = 2,
        color = ~pal(category),    # border
        opacity = 1,
        fillColor = ~pal(category),
        fillOpacity = 0.5,         # semi-transparent fill
        popup = ~paste0("<strong>", name, "</strong><br>", category)
      )
  })
  
  # Static legend
  output$legend <- renderLeaflet({
    leaflet() %>%
      addLegend(
        position = "bottomright",
        pal = pal,
        values = categories,
        title = "Site Type"
      )
  })
}

shinyApp(ui, server)

############################## deploy via rsconnect ##############################

# Only run this in terminal to deploy
# library(rsconnect)
# rsconnect::deployApp(appDir = ".", appName = "coral-restoration")

############################## archive ##############################
