# For loops with Rocio

ysi <- read_excel(here("water_quality", "data_raw", "Water quality data.xlsx"), sheet = "ysi") %>% 
  rename_with(tolower) %>% # prevents clean_names() from adding extra underscores before caps
  clean_names() %>%
  mutate(site_code = if_else(site_code == "Corington", "COV",
                             if_else(site_code == "greenhEB", "GEB",
                                     if_else(site_code == "1gic", "GIC1",
                                             if_else(site_code == "3gic", "GIC3",
                                                     if_else(site_code == "1yic", "YIC1",
                                                             if_else(site_code == "3yic", "YIC3",
                                                                     site_code # can remove a lot of these now that data is corrected
                                                             ))))))) %>%
  rename(site_code_depth = site_code)

# Removing duplicates

ysi1 <- ysi %>%
  drop_na(turbidity_fnu)

duplicates <- ysi1 %>%
  group_by(site_code_depth, date) %>%
  summarize(count = n()) %>%
  filter(count >= 2)

## Test removing first duplicate value
i <- 1
inddup <- which(ysi1$date == duplicates$date[i] & ysi1$site_code_depth == duplicates$site_code_depth[i])
a <- ysi1[inddup,]
ind2del <- inddup[2]
ysi1 <- ysi1[-ind2del,]

## for loop to remove all duplicate values
for(i in 1:length(duplicates$site_code_depth)) {
  print(i)
  inddup <- which(ysi1$date == duplicates$date[i] & ysi1$site_code_depth == duplicates$site_code_depth[i])
  ind2del <- inddup[2]
  ysi1 <- ysi1[-ind2del,]
}

## alt option for lapply