######################## Day 2- R for Environmental Data Analysis Workshop
######################## Rocio Prieto Gonzalez 2025/06/15 --- Counting Whales!


#####  Load libraries
library(readxl)       # Read xls and xlsx files
library(janitor)      # clean_names() and handle case formatting
library(snakecase)    # convert strings to any case formatting
library(dplyr)        # very compact code, more difficult to read until you are used to =)
library(ggplot2)
library(reshape2)     # to use melt() to formate data wide/long
library(RColorBrewer) # pretty colors
library(car)          # levene test (equal variance)
library(dunn.test)    # dunn test 


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#################   functions   #################
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

CV <- function(x){
  CV <- sd(x)/mean(x) *100
  return(CV)
}