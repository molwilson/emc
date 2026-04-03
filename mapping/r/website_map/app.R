library(tidyverse)
library(shiny)
library(leaflet)
library(readxl)
library(janitor)
library(lubridate)
library(sf)
library(later)

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
         category = "coral collection") %>%
  select(name = genotype, category, latitude = lat, longitude = lon, date, year) # year

site_dat <- read_excel("Genotype tracking.xlsx", sheet = "site locations") %>%
  clean_names() %>%
  mutate(date = as.Date(date),
         year = year(ymd(date))
  ) %>%
  bind_rows(collection_dat) %>%
  mutate(category = str_to_title(category))


############################## Shiny App #################################

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

      actionButton(
        "play_anim",
        label = "▶  Click to watch our growth!",
        width = "100%"
      ),

      sliderInput(
        inputId = "date",
        label = "Date:",
        min = min(site_dat$date, na.rm = TRUE),
        max = max(site_dat$date, na.rm = TRUE),
        value = max(site_dat$date, na.rm = TRUE),
        timeFormat = "%m/%d/%y",
        width = "100%"
      ),

      # Placeholder for photo/description panel
      uiOutput("nursery_info")
    ),

    mainPanel(
      leafletOutput("map", height = 600)
    )
  )
)

server <- function(input, output, session) {

  categories <- unique(site_dat$category)

  pal <- colorFactor(
    palette = c("#FEE100", "#FF284B", "#FF7E5A"),
    domain = categories
  )

  # Custom HTML legend function
  addLegendCustom <- function(map, position = "bottomright") {
    legend_html <- "
  <div style='background: white; padding: 10px; border-radius: 5px;'>

    <div style='display: flex; align-items: center; margin-bottom: 5px;'>
      <div style='width:6px; height:6px; border:1px solid #FEE100; border-radius:50%; background-color:#FEE100; opacity:0.7; margin-right:5px;'></div>
      <span>Collection</span>
    </div>

    <div style='display: flex; align-items: center; margin-bottom: 5px;'>
      <div style='width:14px; height:14px; border:2px solid #FF7E5A; border-radius:50%; background-color:#FF7E5A; opacity:0.7; margin-right:5px;'></div>
      <span>Restoration</span>
    </div>

    <div style='display: flex; align-items: center; margin-bottom: 5px;'>
      <div style='width:14px; height:14px; border:2px solid #FF284B; border-radius:50%; background-color:#FF284B; opacity:0.7; margin-right:5px;'></div>
      <span>Nursery</span>
    </div>

  </div>
  "

    addControl(map, html = legend_html, position = position)
  }

  # Reactive filter: cumulative points up to selected date
  filtered_data <- reactive({
    site_dat %>%
      filter(
        category %in% input$category,
        date <= input$date
      )
  })

  # Initial map
  output$map <- renderLeaflet({
    leaflet(options = leafletOptions(preferCanvas = TRUE)) %>%
      addProviderTiles("Esri.WorldImagery") %>%
      setView(lng = -61.8, lat = 17.08, zoom = 11.4) %>%
      addLegendCustom()
  })

  # Dynamic markers
  observe({
    fd <- filtered_data()

    leafletProxy("map") %>%
      clearMarkers() %>%
    #   clearControls() %>%

      # collection sites
      addCircleMarkers(
        data = fd %>% filter(category == "Collection"),
        lng = ~longitude, lat = ~latitude,
        radius = 3, stroke = FALSE,
        fillColor = ~pal(category),
        fillOpacity = 0.7,
        popup = ~paste0("<strong>", name, "</strong><br>", category)
      ) %>%

      # nursery + restoration
      addCircleMarkers(
        data = fd %>% filter(category != "Collection"),
        lng = ~longitude, lat = ~latitude,
        radius = 7, stroke = TRUE, weight = 2,
        color = ~pal(category),
        opacity = 1, fillColor = ~pal(category),
        fillOpacity = 0.5,
        popup = ~paste0("<strong>", name, "</strong><br>", category),
        layerId = ~name
      )
      # %>%
      # addLegendCustom()
 })



  # Animate by date using later
  observeEvent(input$play_anim, {

    dates <- sort(unique(site_dat$date))

    animate_date <- function(i) {
      if (i > length(dates)) return()  # stop at the end

      # Update slider to current date
      updateSliderInput(session, "date", value = dates[i], timeFormat = "%m/%d/%y")

      # Schedule next date (adjust 0.2 for speed, lower = faster)
      later::later(function() {
        animate_date(i + 1)
      }, 0.2)
    }

    animate_date(1)
  })

  # Legend
  output$legend <- renderLeaflet({
    leaflet() %>%
      addLegend(
        position = "bottomright",
        pal = pal,
        values = categories,
        title = "Site Type"
      )
  })

  # Show nursery photo/description when clicked
  observeEvent(input$map_marker_click, {
    click <- input$map_marker_click

    # Check that click$id exists
    if (is.null(click$id)) {
      output$nursery_info <- renderUI({ NULL })  # hide panel
      return()
    }

    # Only proceed if the clicked marker is a nursery
    nursery <- site_dat %>%
      filter(name == click$id, category == "Nursery")

    if (nrow(nursery) == 0 || is.na(nursery$photo[1])) {
      output$nursery_info <- renderUI({ NULL })  # hide panel
    } else {
      output$nursery_info <- renderUI({
        div(
          style = "text-align: center; margin-top: 20px;",
          h4(nursery$name),
          img(
            src = nursery$photo[1],
            style = "width:90%; max-width:300px; margin-bottom:10px;"
          ),
          p(
            nursery$description[1],
            style = "max-width:300px; margin:auto;"
          )
        )
      })
    }
  })

}

shinyApp(ui, server)















############################## deploy via rsconnect ##############################

# Only run this in terminal to deploy
# library(rsconnect)
# rsconnect::deployApp(appDir = ".", appName = "coral-restoration")

############################## archive ##############################



# multi photo version

# ui <- fluidPage(
#   titlePanel("AnuBlue Coral Restoration"),
#   
#   sidebarLayout(
#     sidebarPanel(
#       checkboxGroupInput(
#         inputId = "category",
#         label = "Select Site Type:",
#         choices = unique(site_dat$category),
#         selected = unique(site_dat$category)
#       ),
#       
#       actionButton(
#         "play_anim",
#         label = "▶  Click to watch our growth!",
#         width = "100%"
#       ),
#       
#       sliderInput(
#         inputId = "date",
#         label = "Date:",
#         min = min(site_dat$date, na.rm = TRUE),
#         max = max(site_dat$date, na.rm = TRUE),
#         value = max(site_dat$date, na.rm = TRUE),
#         timeFormat = "%b %Y",
#         width = "100%"
#       ),
#       
#       # Placeholder for photo/description panel
#       uiOutput("nursery_info")
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
#   categories <- unique(site_dat$category)
#   
#   pal <- colorFactor(
#     palette = c("#FEE100", "#FF284B", "#FF7E5A"),
#     domain = categories
#   )
#   
#   # Custom HTML legend function
#   addLegendCustom <- function(map, position = "bottomright") {
#     legend_html <- "
#   <div style='background: white; padding: 10px; border-radius: 5px;'>
#     <div style='display: flex; align-items: center; margin-bottom: 5px;'>
#       <div style='width:6px; height:6px; border:1px solid #FEE100; border-radius:50%; background-color:#FEE100; opacity:0.7; margin-right:5px;'></div>
#       <span>Collection</span>
#     </div>
#     <div style='display: flex; align-items: center; margin-bottom: 5px;'>
#       <div style='width:14px; height:14px; border:2px solid #FF7E5A; border-radius:50%; background-color:#FF7E5A; opacity:0.7; margin-right:5px;'></div>
#       <span>Restoration</span>
#     </div>
#     <div style='display: flex; align-items: center; margin-bottom: 5px;'>
#       <div style='width:14px; height:14px; border:2px solid #FF284B; border-radius:50%; background-color:#FF284B; opacity:0.7; margin-right:5px;'></div>
#       <span>Nursery</span>
#     </div>
#   </div>
#   "
#     
#     addControl(map, html = legend_html, position = position)
#   }
#   
#   # Reactive filter: cumulative points up to selected date
#   filtered_data <- reactive({
#     site_dat %>%
#       filter(
#         category %in% input$category,
#         date <= input$date
#       )
#   })
#   
#   # Initial map
#   output$map <- renderLeaflet({
#     leaflet(options = leafletOptions(preferCanvas = TRUE)) %>%
#       addProviderTiles("Esri.WorldImagery") %>%
#       setView(lng = -61.8, lat = 17.08, zoom = 11.4) %>%
#       addLegendCustom()
#   })
#   
#   # Dynamic markers
#   observe({
#     fd <- filtered_data()
#     
#     leafletProxy("map") %>%
#       clearMarkers() %>%
#       
#       # collection sites
#       addCircleMarkers(
#         data = fd %>% filter(category == "Collection"),
#         lng = ~longitude, lat = ~latitude,
#         radius = 3, stroke = FALSE,
#         fillColor = ~pal(category),
#         fillOpacity = 0.7,
#         popup = ~paste0("<strong>", name, "</strong><br>", category)
#       ) %>%
#       
#       # nursery + restoration
#       addCircleMarkers(
#         data = fd %>% filter(category != "Collection"),
#         lng = ~longitude, lat = ~latitude,
#         radius = 7, stroke = TRUE, weight = 2,
#         color = ~pal(category),
#         opacity = 1, fillColor = ~pal(category),
#         fillOpacity = 0.5,
#         popup = ~paste0("<strong>", name, "</strong><br>", category),
#         layerId = ~name
#       )
#   })
#   
#   # Animate by date using later
#   observeEvent(input$play_anim, {
#     dates <- sort(unique(site_dat$date))
#     
#     animate_date <- function(i) {
#       if (i > length(dates)) return()  # stop at the end
#       updateSliderInput(session, "date", value = dates[i])
#       later::later(function() {
#         animate_date(i + 1)
#       }, 0.2)
#     }
#     
#     animate_date(1)
#   })
#   
#   # Legend
#   output$legend <- renderLeaflet({
#     leaflet() %>%
#       addLegend(
#         position = "bottomright",
#         pal = pal,
#         values = categories,
#         title = "Site Type"
#       )
#   })
#   
#   # ---- Multi-photo slider for nursery ----
#   
#   # Reactive value to track current photo index
#   photo_index <- reactiveVal(1)
#   
#   # Show nursery photo/description when clicked
#   observeEvent(input$map_marker_click, {
#     click <- input$map_marker_click
#     
#     # Check that click$id exists
#     if (is.null(click$id)) {
#       output$nursery_info <- renderUI({ NULL })
#       return()
#     }
#     
#     # Only proceed if the clicked marker is a nursery
#     nursery <- site_dat %>%
#       filter(name == click$id, category == "Nursery")
#     
#     # Reset photo index
#     photo_index(1)
#     
#     if (nrow(nursery) == 0 || is.na(nursery$photo[1])) {
#       output$nursery_info <- renderUI({ NULL })
#     } else {
#       output$nursery_info <- renderUI({
#         # Split comma-separated photos
#         photos <- strsplit(nursery$photo[1], ",")[[1]]
#         i <- photo_index()
#         i <- max(1, min(i, length(photos)))  # clamp
#         
#         div(
#           style = "text-align: center; margin-top: 20px;",
#           h4(nursery$name),
#           img(
#             src = photos[i],
#             style = "width:90%; max-width:300px; margin-bottom:10px;"
#           ),
#           # Buttons to cycle photos
#           div(
#             style = "display:flex; justify-content: space-between;",
#             actionButton("prev_img", "◀", width = "45%"),
#             actionButton("next_img", "▶", width = "45%")
#           ),
#           # Optional counter
#           p(paste(i, "/", length(photos))),
#           p(
#             nursery$description[1],
#             style = "max-width:300px; margin:auto;"
#           )
#         )
#       })
#     }
#   })
#   
#   # Next / previous photo buttons
#   observeEvent(input$next_img, {
#     photo_index(photo_index() + 1)
#   })
#   
#   observeEvent(input$prev_img, {
#     photo_index(photo_index() - 1)
#   })
#   
#   # Make the photo UI reactive to changes in photo_index
#   observe({
#     i <- photo_index()
#     click <- input$map_marker_click
#     if (is.null(click$id)) return()
#     
#     nursery <- site_dat %>%
#       filter(name == click$id, category == "Nursery")
#     
#     if (nrow(nursery) == 0 || is.na(nursery$photo[1])) return()
#     
#     photos <- strsplit(nursery$photo[1], ",")[[1]]
#     i <- max(1, min(i, length(photos)))
#     
#     output$nursery_info <- renderUI({
#       div(
#         style = "text-align: center; margin-top: 20px;",
#         h4(nursery$name),
#         img(
#           src = photos[i],
#           style = "width:90%; max-width:300px; margin-bottom:10px;"
#         ),
#         div(
#           style = "display:flex; justify-content: space-between;",
#           actionButton("prev_img", "◀", width = "45%"),
#           actionButton("next_img", "▶", width = "45%")
#         ),
#         p(paste(i, "/", length(photos))),
#         p(
#           nursery$description[1],
#           style = "max-width:300px; margin:auto;"
#         )
#       )
#     })
#   })
#   
# }
# 
# shinyApp(ui, server)
