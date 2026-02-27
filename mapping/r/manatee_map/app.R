library(tidyverse)
library(shiny)
library(leaflet)
library(readxl)
library(janitor)
library(lubridate)
library(sf)

################# set wd outside of app code #################
# setwd("~/Github/emc/mapping/r/manatee_map")

################################## data ##################################

site_dat <- read_excel("Manatee sightings.xlsx", sheet = "sightings") %>%
  clean_names() %>%
  filter(island == "Antigua") %>%
  mutate(date = as.Date(date)) %>%
  select(name = location, latitude, longitude, date) %>%
  group_by(longitude, latitude) %>% # jittering duplicated sites
  mutate(count = n(),
         longitude = if_else(count > 1, jitter(longitude, factor = 0.005), longitude),
         latitude  = if_else(count > 1, jitter(latitude,  factor = 0.005), latitude)) %>%
  ungroup()

################################## app ##################################

ui <- fluidPage(
  titlePanel("Manatee sightings - Antigua"),

  # 🔹 Slider at the top
  sliderInput(
    inputId = "date",
    label = "Select Date:",
    min = min(site_dat$date, na.rm = TRUE),
    max = max(site_dat$date, na.rm = TRUE),
    value = max(site_dat$date, na.rm = TRUE),
    timeFormat = "%b %d, %Y",
    width = "100%",
    animate = animationOptions(
      interval = 25,   # 0.05 second per step
      loop = FALSE,
      playButton = icon("play"),
      pauseButton = icon("pause")
    )
  ),

  # 🔹 Map below slider
  leafletOutput("map", height = 600)
)

server <- function(input, output, session) {

  # Reactive filter: cumulative points up to selected date
  filtered_data <- reactive({
    site_dat %>%
      filter(date <= input$date)
  })

  # Initial map with fixed view
  output$map <- renderLeaflet({
    leaflet(options = leafletOptions(preferCanvas = TRUE)) %>%
      addProviderTiles("Esri.WorldImagery") %>%
      setView(
        lng = -61.8,
        lat = 17.08,
        zoom = 11
      )
  })

  # Dynamic markers
  observe({
    fd <- filtered_data()

    leafletProxy("map") %>%
      clearMarkers() %>%
      addCircleMarkers(
        data = fd,
        lng = ~longitude,
        lat = ~latitude,
        radius = 6,
        stroke = TRUE,
        weight = 2,
        color = "#FF284B",
        opacity = 1,
        fillColor = "#FF284B",
        fillOpacity = 0.5,
        popup = ~paste0("<strong>", name, "</strong><br>",
                        "Date: ", date)
      )
  })
}

shinyApp(ui, server)

# ui <- fluidPage(
#   titlePanel("Manatee sightings - Eastern Caribbean"),
#   
#   # 🔹 Slider at the top
#   
#   sliderInput(
#     inputId = "date",
#     label = "Select Date:",
#     min = min(site_dat$date, na.rm = TRUE),
#     max = max(site_dat$date, na.rm = TRUE),
#     value = max(site_dat$date, na.rm = TRUE),
#     timeFormat = "%b %d, %Y",
#     width = "100%",   # 🔹 full width
#     animate = animationOptions(
#       interval = 50,
#       loop = FALSE,
#       playButton = icon("play"),
#       pauseButton = icon("pause")
#     )
#   ),
#   
#   # 🔹 Map below slider
#   leafletOutput("map", height = 600)
# )
# 
# server <- function(input, output, session) {
#   
#   # Reactive filter: cumulative points up to selected date
#   filtered_data <- reactive({
#     site_dat %>%
#       filter(date <= input$date)
#   })
#   
#   # Initial map
#   output$map <- renderLeaflet({
#     leaflet(options = leafletOptions(preferCanvas = TRUE)) %>%
#       addProviderTiles("Esri.WorldImagery")
#   })
#   
#   # Dynamic markers with auto-zoom
#   observe({
#     fd <- filtered_data()
#     
#     proxy <- leafletProxy("map") %>%
#       clearMarkers() %>%
#       addCircleMarkers(
#         data = fd,
#         lng = ~longitude,
#         lat = ~latitude,
#         radius = 6,
#         stroke = TRUE,
#         weight = 2,
#         color = "#FF284B",
#         opacity = 1,
#         fillColor = "#FF284B",
#         fillOpacity = 0.5,
#         popup = ~paste0("<strong>", name, "</strong><br>",
#                         "Date: ", date)
#       )
#     
#     # Auto-zoom/pan to visible points
#     if(nrow(fd) > 0){
#       proxy %>%
#         fitBounds(
#           lng1 = min(fd$longitude),
#           lat1 = min(fd$latitude),
#           lng2 = max(fd$longitude),
#           lat2 = max(fd$latitude)
#         )
#     }
#   })
# }
# 
# shinyApp(ui, server)


# ui <- fluidPage(
#   titlePanel("Manatee sightings - Eastern Caribbean"),
# 
#   sidebarLayout(
#     sidebarPanel(
#       # Animated date slider
#       sliderInput(
#         inputId = "date",
#         label = "Select Date:",
#         min = min(site_dat$date, na.rm = TRUE),
#         max = max(site_dat$date, na.rm = TRUE),
#         value = max(site_dat$date, na.rm = TRUE),
#         timeFormat = "%b %d, %Y",
#         animate = animationOptions(
#           interval = 50,   # 1 second per step
#           loop = FALSE,
#           playButton = icon("play"),
#           pauseButton = icon("pause")
#         )
#       )
#     ),
# 
#     mainPanel(
#       leafletOutput("map", height = 600)
#     )
#   )
# )
# 
# server <- function(input, output, session) {
# 
#   # Reactive filter: cumulative points up to selected date
#   filtered_data <- reactive({
#     site_dat %>%
#       filter(date <= input$date)
#   })
# 
#   # Initial map
#   output$map <- renderLeaflet({
#     leaflet(options = leafletOptions(preferCanvas = TRUE)) %>%
#       addProviderTiles("Esri.WorldImagery") %>%
#       setView(
#         lng = -61.8,
#         lat = 17.08,
#         zoom = 11.5
#     )
#   })
# 
#   # Dynamic markers
#   observe({
#     fd <- filtered_data()
# 
#     leafletProxy("map") %>%
#       clearMarkers() %>%
#       addCircleMarkers(
#         data = fd,
#         lng = ~longitude,
#         lat = ~latitude,
#         radius = 5,
#         stroke = TRUE,
#         weight = 2,
#         color = "#FF284B",
#         opacity = 1,
#         fillColor = "#FF284B",
#         fillOpacity = 0.5,
#         popup = ~paste0("<strong>", name, "</strong><br>",
#                         "Date: ", date)
#       )
#   })
# }
# 
# shinyApp(ui, server)

############################## deploy via rsconnect ##############################

# Only run this in terminal to deploy
# library(rsconnect)
# rsconnect::deployApp(appDir = ".", appName = "antigua-manatee")