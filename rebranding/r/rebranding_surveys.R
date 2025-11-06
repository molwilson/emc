library(tidyverse)
library(janitor)
library(readxl)
library(ggrepel)

# data import and formatting

responses <- read_excel(here("rebranding", "data", "Rebranding Survey  (Responses).xlsx"), sheet = "Form Responses 1") 

response_counts <- responses %>%
  summarize(across(everything(), ~ sum(!is.na(.x)))) %>%
  pivot_longer(everything(), names_to = "question", values_to = "n_responses")


# Q1

q1_data <- responses %>%
  select(2) %>%
  rename(answer_raw = 1) %>%
  filter(!is.na(answer_raw))

n_respondents_q1 <- nrow(q1_data)

q1_summary <- q1_data %>%
  count(answer_raw, sort = TRUE) %>%
  mutate(percent = n / n_respondents_q1 * 100)

q1_colors <- c("indianred1", "lightblue")  # adjust to match number of categories

ggplot(q1_summary, aes(x = "", y = percent, fill = answer_raw)) +
  geom_col(width = 1, color = "white") +
  coord_polar(theta = "y") +
  geom_text(aes(label = paste0(round(percent, 1), "%")),
            position = position_stack(vjust = 0.5), size = 4) +
  scale_fill_manual(values = q1_colors[1:nrow(q1_summary)]) +
  labs(
    title = str_wrap(
      paste0(names(responses)[2]),
      width = 40  # wrap roughly every 40 characters
    ),
    subtitle = paste0("(n = ", n_respondents_q1, " respondents)"),
    fill = "Response"
  ) +
  theme_void() +
  theme(#plot.background = element_rect(fill = "white", color = NA),
        plot.title = element_text(face = "bold", size = 10, hjust = 0.5),
        plot.subtitle = element_text(size = 9, hjust = 0.5),
        legend.position = "bottom",        # move legend to bottom
        legend.title = element_blank())

ggsave(here("rebranding", "figs", "Q1.png"), width = 4, height = 3)

# Q2

q2_data <- responses %>%
  select(3) %>%
  rename(answer_raw = 1) %>%
  filter(!is.na(answer_raw)) %>%
  mutate(answer_raw = case_when(answer_raw == "And blue (u silent)" ~ "Other",
                                answer_raw == "Read as one word" ~ "A New Blue",
                                answer_raw == "Antigua blue" ~ "Antigua Blue",
                                answer_raw == "Spell out ANU" ~ "A.N.U. Blue"
  ))

n_respondents_q2 <- nrow(q2_data)

q2_summary <- q2_data %>%
  count(answer_raw, sort = TRUE) %>%
  mutate(percent = n / n_respondents_q2 * 100)

q2_colors <- c("lightblue3", "indianred1", "goldenrod1", "lavenderblush2")  # adjust to match number of categories

ggplot(q2_summary, aes(x = "", y = percent, fill = answer_raw)) +
  geom_col(width = 1, color = "white") +
  coord_polar(theta = "y") +
  # Labels inside slices for bigger percentages
  geom_text(aes(label = ifelse(percent > 5, paste0(round(percent,1), "%"), "")),
            position = position_stack(vjust = 0.5), size = 4) +
  # Labels outside for small slices
  geom_text_repel(
    data = q2_summary %>% filter(percent <= 5),
    aes(x = "", y = cumsum(percent) - percent / 2, label = paste0(round(percent,1), "%")),
    nudge_x = .6,          # move outside the pie
    direction = "y",
    segment.color = "grey50",
    size = 4,
    inherit.aes = FALSE
  ) +
  scale_fill_manual(values = q2_colors[1:nrow(q2_summary)]) +
  labs(
    title = str_wrap(
      paste0(names(responses)[3]),
      width = 40  # wrap roughly every 40 characters
    ),
    subtitle = paste0("(n = ", n_respondents_q2, " respondents)"),
    fill = "Response"
  ) +
  theme_void() +
  theme(#plot.background = element_rect(fill = "white", color = NA),
        plot.title = element_text(face = "bold", size = 10, hjust = 0.5),
        plot.subtitle = element_text(size = 9, hjust = 0.5),
        legend.position = "right",        # move legend to bottom
        legend.title = element_blank())

ggsave(here("rebranding", "figs", "Q2.png"), width = 4, height = 3.2)


# Q3

n_respondents_q3 <- sum(!is.na(responses[[4]]))

responses_q3 <- responses %>%
  select(4) %>%
  rename(answer_raw = 1) %>%
  filter(!is.na(answer_raw)) %>%
  # remove parentheses and their contents (e.g. "(beach, ocean)")
  mutate(answer_raw = str_remove_all(answer_raw, "\\s*\\([^\\)]*\\)")) %>%
  # trim extra spaces just in case
  mutate(answer_raw = str_trim(answer_raw)) %>%
  separate_rows(answer_raw, sep = ",\\s*") %>%
  filter(!answer_raw %in% c("Ocean restoration helps get message across", "Start a new", "Make the ocean better/how it was")) %>%
  count(answer_raw, sort = TRUE) %>%
  mutate(percent = n / n_respondents_q3 * 100)

question_text <- names(responses)[4]  # use original question as title

ggplot(responses_q3, aes(
  x = forcats::fct_reorder(answer_raw, percent),
  y = percent,
  fill = answer_raw
)) +
  geom_col(fill = "lightblue3") +
  geom_text(aes(label = paste0(round(percent, 1), "%")),
            vjust = -0.5, size = 3.5) +
  ylim(0, 100) +
  labs(
    title = str_wrap("3. When you see this name, what kind of organization do you think it is or what do you imagine it does?", width = 55),   # wrap long question text
    subtitle = paste0("(n = ", n_respondents_q3, " respondents)"),  # subtitle with n
    x = NULL,
    y = "Percent of respondents"
  ) +
  theme_minimal(base_size = 12) +
  theme(
    legend.position = "none",
    plot.title = element_text(face = "bold", size = 13, hjust = 0.5),
    plot.subtitle = element_text(size = 11, hjust = 0.5),
    axis.text.x = element_text(size = 11, angle = 45, hjust = 1)
  )

ggsave(here("rebranding", "figs", "Q3.png"), width = 8, height = 5)

