library(patchwork) # for stacking graphs

# Import data

shark_depth <- read_excel(here("tagging", "data_raw", "Shark Tagging Data.xlsx"), sheet = "Depth") %>% 
  clean_names() %>%
  left_join(read_excel(here("tagging", "data_raw", "Shark Tagging Data.xlsx"), sheet = "Temp") %>%
              clean_names() %>%
              select(pttid, date, sst = sea_surface_temp_c, adt = at_depth_temp_c),
            by = c("pttid", "date"))


# Temperature 

p_temp <- ggplot(shark_depth, aes(x = date, group = pttid)) +
  geom_line(data = shark_depth %>%
              pivot_longer(cols = c(sst, adt),
                           names_to = "type", values_to = "temp") %>%
              mutate(type = recode(type,
                                   sst = "Sea surface temp.",
                                   adt = "At-depth temp.")),
            aes(y = temp, color = type, group = interaction(pttid, type)),
            size = 0.4) +
  scale_color_manual(values = c("Sea surface temp." = "coral",
                                "At-depth temp." = "goldenrod")) +
  theme_bw() +
  theme(legend.position = "top") +
  labs(color = NULL, y = "Temperature (C)", x = "")
ggsave(here("tagging", "figs", "shark_temp.png"), plot = p_temp, width = 4, height = 4)


# Depth

p_depth <- ggplot(shark_depth, aes(x = date)) +
  geom_ribbon(aes(ymin = min_depth_m, ymax = max_depth_m, group = pttid),
              fill = "gray", alpha = 0.4) +
  geom_line(data = shark_depth %>%
              pivot_longer(cols = c(min_depth_m, mean_depth_m, max_depth_m),
                           names_to = "type", values_to = "depth") %>%
              mutate(type = recode(type,
                                   min_depth_m = "Min. depth",
                                   mean_depth_m = "Mean depth",
                                   max_depth_m = "Max. depth")),
            aes(y = depth, color = type, group = interaction(pttid, type)),
            size = 0.8) +
  scale_y_reverse() +
  scale_color_manual(values = c("Min. depth" = "lightblue",
                                "Mean depth" = "gray50",
                                "Max. depth" = "steelblue")) +
  theme_bw() +
  theme(legend.position = "top") +
  labs(color = "", y = "Depth (m)", x = "")
ggsave(here("tagging", "figs", "shark_depth.png"), plot = p_depth, width = 4, height = 4)


# Plot panels
p_overlay <- p_temp / p_depth
ggsave(here("tagging", "figs", "depth_temp.png"), plot = p_overlay, width = 6, height = 6)


# animations
p_anim <- p_depth_overlay +
  theme_bw(base_size = 18) +
  transition_reveal(date)
anim <- gganimate::animate(p_anim, nframes = 100, fps = 10, width = 800, height = 600)
anim_save(here("tagging", "figs", "shark_depth.gif"), animation = anim)

p_anim <- p_depth +
  facet_wrap(~ pttid) +
  theme_bw(base_size = 18) +
  theme(legend.position = "top") +
  transition_reveal(date)
anim <- gganimate::animate(p_anim, nframes = 100, fps = 10, width = 800, height = 600)
anim_save(here("tagging", "figs", "shark_depth_indv.gif"), animation = anim)


  
  
