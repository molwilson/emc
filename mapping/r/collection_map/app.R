library(tidyverse)
library(shiny)
library(shinyjs)
library(rsconnect)
library(readxl)
library(janitor)
library(lubridate)
library(ggplot2)
library(sf)

# setwd("mapping/r")
# rsconnect::deployApp("collection_map")
collection_dat <- read_excel("Genotype tracking.xlsx", sheet = "collection") %>% 
  clean_names() %>%
  filter(!lat %in% c("NA", "n/a")) %>%
  select(genotype, date, lat, lon) %>%
  mutate(date = as.Date(as.numeric(date), origin = "1899-12-30"),
         year = year(ymd(date)),
         species_code = substr(genotype, 1, 4),
         species = case_when(substr(genotype, 1, 4) == "ACER" ~ "A. cervicornis",
                             substr(genotype, 1, 4) == "APRO" ~ "A. prolifera",
                             substr(genotype, 1, 4) == "APAL" ~ "A. palmata",
                             substr(genotype, 1, 4) == "PPOR" ~ "P. porites",
                             substr(genotype, 1, 4) == "PFUR" ~ "P. furcata",
                             substr(genotype, 1, 4) == "PDIV" ~ "P. divaricata",
                             substr(genotype, 1, 4) == "OANN" ~ "O. annularis",
                             substr(genotype, 1, 4) == "OFAV" ~ "O. faveolata",
                             substr(genotype, 1, 4) == "OFRA" ~ "O. franksi",
                             substr(genotype, 1, 4) == "PSTR" ~ "P. strigosa",
                             substr(genotype, 1, 4) == "PCLI" ~ "P. clivosa",
                             substr(genotype, 1, 4) == "CNAT" ~ "C. natans",
                             substr(genotype, 1, 4) == "DLAB" ~ "D. labyrinthiformis",
                             substr(genotype, 1, 4) == "DCYL" ~ "D. cylindrus",
                             substr(genotype, 1, 4) == "MCAV" ~ "M. cavernosa")) %>%
  mutate(across(c(lat, lon), as.numeric))

lon_range <- range(collection_dat$lon)
lat_range <- range(collection_dat$lat)

atg <- readRDS("atg_union.rds")

ui <- fluidPage(
  useShinyjs(),   # enable JS
  titlePanel("Coral collections"),
  sidebarLayout(
    sidebarPanel(
      sliderInput(
        "year", "Select year:",
        min = min(collection_dat$year),
        max = max(collection_dat$year),
        value = min(collection_dat$year),
        step = 1,
        sep = "",
        animate = animationOptions(
          interval = 1000,  # 1000 ms = 1 second
          loop = TRUE
        )
      )
    ),
    mainPanel(
      plotOutput("mapPlot", height = "600px")
    )
  )
)

server <- function(input, output, session) {
  
  # autoplay the slider when app loads
  session$onFlushed(function() {
    runjs("document.querySelector('.slider-animate-button').click();")
  }, once = TRUE)
  
  output$mapPlot <- renderPlot({
    ggplot() +
      geom_sf(data = atg, fill = "tan", color = "tan") +
      geom_point(
        data = subset(collection_dat, year <= input$year),
        aes(x = lon, y = lat, color = species),
        alpha = 0.7
      ) +
      coord_sf(xlim = lon_range, ylim = lat_range) +
      scale_x_continuous(expand = expansion(mult = c(.1, .15))) +
      scale_y_continuous(expand = expansion(mult = c(.15, .1))) +
      labs(x = NULL, y = NULL,
           title = paste("Year:", input$year)) +
      theme_bw() +
      theme(
        panel.background = element_rect(fill = "lightblue2"),
        panel.grid = element_blank(),
        legend.position = "right",
        legend.key.size = unit(0.5, "cm"),
        legend.key.width = unit(1.5, "cm"),
        legend.spacing.y = unit(0.2, "cm"),
        legend.title = element_text(size = 10),
        legend.text = element_text(size = 9)
      )
  })
}

shinyApp(ui = ui, server = server)