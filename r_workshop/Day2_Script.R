######################## Day 2- R for Environmental Data Analysis Workshop
######################## Rocio Prieto Gonzalez 2025/06/15 --- Counting Whales!


# loading functions
src_R.loc <- "/Users/margaretwilson/Github/emc/r_workshop" 
setwd(src_R.loc)   # dir() 
source("Day2_script_fun.R")

# loading data
dat.loc <- "/Users/margaretwilson/Github/emc/agrra_monitoring/data_raw/ATG_NEMMA_2025/Calculated" 
setwd(dat.loc)


## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
##         Bonus: Listing functions in R       ----
## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

  
# 1. List all objects in base package
ls("package:base")

# 2. Show only functions (exclude data/constants)
objs <- ls("package:base")
funs <- objs[sapply(objs, function(x) is.function(get(x, "package:base")))]
head(funs, 20)   # show first 20 functions

# 3. List functions in any package (for exemaple stats)
ls("package:stats")





## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
##                 Organizing data             ----
## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 


# Let's have a look at the Overall data sheet

# 0. Read the Excel file
# loading data
dat.loc <- "D:\\Dropbox\\Counting whales\\Proyectos-Colaboraciones\\Antigua\\R workshop\\EMC data\\" 
setwd(dat.loc)
dat <- read_excel("BenthicCoverScaledByTransect.xlsx",sheet = "Overall", trim_ws = TRUE)  

## Other useful arguments:
##      skip = 8,   # start reading from line 9
##      trim_ws = TRUE,   Should leading and trailing whitespace be trimmed? (by default is TRUE-- super useful)

head(dat)
tail(dat)
dim(dat)
summary(dat)
str(dat)
names(dat)
# glimpse(dat)  # needs dplyr package



######  ~> cleaning process  ----
###### ~~~~~~~~~~~~~~~~~~~~~~


# 1. Clean column names (all lower case with underscores)
clean_names(dat)
names(dat)
## mmm... what if I want to keep the capitals for categories? 
names(dat)[1:6] <- make_clean_names(names(dat)[1:6])

# 2. Exploring each column
table(dat$transect_id)
unique(dat$transect_id)
table(dat$survey_name)
is.na(dat$survey_name)
any(is.na(dat$survey_name)) # are there any NAs
which(is.na(dat$survey_name))
table(dat$surveyor)
table(dat$surveyed)

# What happen on the days we have 2 surveys at the same time? 
# let's have a look...

# Find duplicated dates
dup_dates <- dat$surveyed[duplicated(dat$surveyed)]
# Show unique dates that are duplicated
dup_dates <- unique(dup_dates)
dup_dates
a <- dat[dat$surveyed == dup_dates,]
## All good =) Two persons doing two different transects ;)

# What if... we have some days with more than 2 lines?
more_than_1_survey <- table(dat$surveyed)[table(dat$surveyed) > 1]


# 3. Remove rows where there are NA (site, lon, lat is NA? so on...)
ind2del <- which(is.na(dat$survey_name))
dat <- dat[-ind2del, ]
# other option to do the same
dat <- dat[!is.na(dat$survey_name), ]

# 4. Fix text formatting (capitalize appropriately)
dat$survey_name <- to_any_case(dat$survey_name, case = "title") 
# case = "title"  --> This Is An Example
# case = "sentence" --> This is an example (useful for species?)



## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
##             Descriptive statistics          ----
## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

# let's see how our data looks like...

# Summarize mean cover per category by site
aggregate(cbind(tCORAL, tMA, tFMA, tCCA, tTA) ~ survey_name, data = dat, mean)
aggregate(cbind(tCORAL, tMA, tFMA, tCCA, tTA) ~ survey_name, data = dat, median)
aggregate(cbind(tCORAL, tMA, tFMA, tCCA, tTA) ~ survey_name, data = dat, sd) # relative to mean
aggregate(cbind(tCORAL, tMA, tFMA, tCCA, tTA) ~ survey_name, data = dat, CV) # sd/mean - easier to interpret without the mean



######  ~> barplot per site  ----
###### ~~~~~~~~~~~~~~~~~~~~~~

###  a) Stacked barplot per site

df_long <- melt(dat, id.vars = c("survey_name", "surveyed"), measure.vars = c("tCORAL","tMA","tFMA","tCCA","tTA"))
head(df_long)
# are the units correct? Is this %? or do we have to multiply *100?

x11() # creates a new window
ggplot(df_long, aes(x = factor(survey_name), y = value, fill = variable)) +
  geom_bar(stat = "identity", position = "stack") +
  theme_minimal() +
  labs(x = "Site", 
       y = "Cover", 
       fill = "Category",
       title = "Benthic community composition per site")

# better?
x11()
ggplot(df_long, aes(x = factor(survey_name), y = value, fill = variable)) +
  geom_bar(stat = "identity", position = "stack") +
  scale_fill_brewer(palette = "Dark2") +
  theme_minimal() +
  labs(x = "Site", 
       y = "Cover", 
       fill = "Category",
       title = "Benthic community composition per site") +
  theme(axis.text.x = element_text(angle = 45, hjust = 1))  # diagonal names

# and now?
x11()
ggplot(df_long, aes(x = factor(survey_name), y = value, fill = variable)) +
  geom_bar(stat = "identity", position = "stack") +
  scale_fill_brewer(palette = "Dark2") +
  theme_minimal() +
  labs(x = "Site", 
       y = "Cover", 
       fill = "Category",
       title = "Benthic community composition per site") +
  theme(
    axis.text.x = element_text(size = 14, angle = 45, hjust = 1),  # text X axis
    axis.text.y = element_text(size = 14),                         # text Y axis
    axis.title.x = element_text(size = 16, face = "bold"),         # title X axis
    axis.title.y = element_text(size = 16, face = "bold"),         # title Y axis
    legend.title = element_text(size = 14, face = "bold"),         # title legend
    legend.text  = element_text(size = 12),                        # text legend
    plot.title   = element_text(size = 18, hjust = 0.5)            # plot title
  )

# two more little things...
# we can make size a parameter ex size <- 16? 
x11()
ggplot(df_long, aes(x = factor(survey_name), y = value, fill = variable)) +
  geom_bar(stat = "identity", position = "stack") +
  scale_fill_brewer(palette = "Dark2") +
  theme_minimal() +
  labs(x = "Site", 
       y = "Cover", 
       fill = "Category",
       title = "Benthic community composition per site") +
  theme(
    axis.text.x = element_text(size = 14, angle = 45, hjust = 1),  # text X axis
    axis.text.y = element_text(size = 14),                         # text Y axis
    axis.title.x = element_text(size = 16, face = "bold"),         # title X axis
    axis.title.y = element_text(size = 16, face = "bold"),         # title Y axis
    legend.title = element_text(size = 14, face = "bold"),         # title legend
    legend.text  = element_text(size = 12),                        # text legend
    plot.title   = element_text(size = 18, hjust = 0.5),           # plot title
    # adjusting marge so we can see all the x-axe
    plot.margin = margin(t = 20, r = 20, b = 30, l = 65, unit = "pt")
  )

setwd(src_R.loc)
ggsave("benthic_plot.png", width = 14, height = 8, dpi = 300)
# Where is it saved?? --> working directory!  



###  b) In parallel barplot per site (side by side)

x11()
ggplot(df_long, aes(x = factor(survey_name), y = value, fill = variable)) +
  geom_bar(stat = "identity", position = position_dodge()) +
  scale_fill_brewer(palette = "Dark2") +
  theme_minimal() +
  labs(x = "Site", 
        y = "Cover", 
        fill = "Category",
        title = "Benthic community composition per site") +
  theme(
      axis.text.x = element_text(size = 14, angle = 45, hjust = 1),  # text X axis
      axis.text.y = element_text(size = 14),                         # text Y axis
      axis.title.x = element_text(size = 16, face = "bold"),         # title X axis
      axis.title.y = element_text(size = 16, face = "bold"),         # title Y axis
      legend.title = element_text(size = 14, face = "bold"),         # title legend
      legend.text  = element_text(size = 12),                        # text legend
      plot.title   = element_text(size = 18, hjust = 0.5),           # plot title
      # adjusting marge so we can see all the x-axe
      plot.margin = margin(t = 20, r = 20, b = 30, l = 40, unit = "pt")
  )



######  ~> other plots?  ----
###### ~~~~~~~~~~~~~~~~~~~~~~


# pie chart

# custom palette (Colors from Molly ;) but you can chose your own...)
cat_palette <- c("coral2", "pink", "darkolivegreen", "darkkhaki", "slategray3")

x11()
ggplot(df_long, aes(x = "", y = value, fill = variable)) +
  geom_bar(width = 1, stat = "identity", color = "black") +
  coord_polar("y", start = 0) +   # pie chart
  # scale_fill_brewer(palette = "Dark2") +
  scale_fill_manual(values = cat_palette) +
  facet_wrap(vars(survey_name), nrow = 2) +  # one pie per site
  labs(title = "Benthic community composition per site",
       fill = "Category") +
  theme_void() +
  theme(
    legend.position = "bottom",
    legend.title = element_text(size = 14, face = "bold"),
    legend.text  = element_text(size = 12),
    plot.title   = element_text(size = 18, hjust = 0.5),
    panel.spacing = unit(1, "lines")
  )


# what if I want a complete area from each category?
head(df_long, 10)
sum_by_cat <- aggregate(, data = dat, sum)

sum_by_cat_site <- aggregate(value ~ survey_name + variable, data = df_long, sum)
sum_by_cat_site


x11()
ggplot(sum_by_cat_site, aes(x = "", y = value, fill = variable)) +
  geom_bar(width = 1, stat = "identity", color = "black") +
  coord_polar("y", start = 0) +   # pie chart
  scale_fill_brewer(palette = "Dark2") +
  # scale_fill_manual(values = cat_palette) +
  facet_wrap(vars(survey_name), nrow = 2) +  # one pie per site
  labs(title = "Benthic community composition per site",
       fill = "Category") +
  theme_void() +
  theme(
    legend.position = "bottom",
    legend.title = element_text(size = 14, face = "bold"),
    legend.text  = element_text(size = 12),
    plot.title   = element_text(size = 18, hjust = 0.5),
    panel.spacing = unit(1, "lines"),
    # adjusting marge so we can see all the x-axe
    plot.margin = margin(t = 20, r = 20, b = 30, l = 50, unit = "pt")
  )



# heat map with the % cover 
#### Important we need to understand the data to know 
# 1st what we want to plot and 
# 2nd if what we are plotting makes sense or not...

x11()
ggplot(df_long, aes(x = factor(survey_name), y = variable, fill = value)) +
  geom_tile() +
  scale_fill_gradient(low = "white", high = "blue", 
                      limits = c(0, 1),
                      breaks = seq(0, 1, by = 0.2)) +
  labs(x = "Site", 
       y = "Category", 
       fill = "Cover",
       title = "Benthic community composition per site") +
  theme_minimal() +
  theme(
    axis.text.x = element_text(size = 14, angle = 60, hjust = 1, face = "bold"),
    axis.text.y = element_text(size = 14, face = "bold"),
    axis.title.x = element_text(size = 16, face = "bold"),
    axis.title.y = element_text(size = 16, face = "bold"),
    legend.title = element_text(size = 14, face = "bold"),
    legend.text = element_text(size = 12),
    plot.title = element_text(size = 18, hjust = 0.5, face = "bold"),
    plot.margin = margin(t = 20, r = 20, b = 60, l = 30, unit = "pt")
  )


x11()
ggplot(sum_by_cat_site, aes(x = factor(survey_name), y = variable, fill = value)) +
  geom_tile() +
  scale_fill_gradient(low = "white", high = "blue", 
                      limits = c(0, 4.5),
                      breaks = seq(0, 4.5, by = 0.5)) +
  labs(x = "Site", 
       y = "Category", 
       fill = "Cover",
       title = "Benthic community composition per site") +
  theme_minimal() +
  theme(
    axis.text.x = element_text(size = 14, angle = 60, hjust = 1, face = "bold"),
    axis.text.y = element_text(size = 14, face = "bold"),
    axis.title.x = element_text(size = 16, face = "bold"),
    axis.title.y = element_text(size = 16, face = "bold"),
    legend.title = element_text(size = 14, face = "bold"),
    legend.text = element_text(size = 12),
    plot.title = element_text(size = 18, hjust = 0.5, face = "bold"),
    plot.margin = margin(t = 20, r = 20, b = 60, l = 30, unit = "pt")
  )

### can we see the difference per years?? 
## Is the %cover increaing or decresing over our survey time and locations?

df_long <- melt(dat, id.vars = c("survey_name", "surveyed"), measure.vars = c("tCORAL","tMA","tFMA","tCCA","tTA"))
unique(df_long$surveyed)



## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
##             Inferential statistics          ----
## ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

######  ~> coral cover per site  ----
###### ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Are there differences in coral cover between sites?

# Boxplot to visualize
x11()
ggplot(dat, aes(x = factor(survey_name), y = tCORAL)) +
  geom_boxplot(fill = "lightblue") +
  theme_minimal() +
  labs(x = "Site", y = "Coral cover", title = "Coral cover across sites") 


x11()
ggplot(dat, aes(x = factor(survey_name), y = tCORAL)) +
  geom_boxplot(fill = "lightblue") +
  theme_minimal() +
  labs(x = "Site", y = "Coral cover", title = "Coral cover across sites") +
  theme(
    axis.text.x = element_text(size = 14, angle = 45, hjust = 1),  # text X axis
    axis.text.y = element_text(size = 14),                         # text Y axis
    axis.title.x = element_text(size = 16, face = "bold"),         # title X axis
    axis.title.y = element_text(size = 16, face = "bold"),         # title Y axis
    legend.title = element_text(size = 14, face = "bold"),         # title legend
    legend.text  = element_text(size = 12),                        # text legend
    plot.title   = element_text(size = 18, hjust = 0.5),           # plot title
    # ajusting marges so we can see all the x-axe
    plot.margin = margin(t = 20, r = 20, b = 30, l = 40, unit = "pt")
  )


# more information about the distribution? 
x11()
ggplot(dat, aes(x = factor(survey_name), y = tCORAL)) +
  geom_violin(fill = "lightblue", trim = FALSE, alpha = 0.6) +  # violin plot
  # dots overlapping?
  geom_jitter(width = 0.2, size = 1, alpha = 0.6, color = "darkblue") +  
  # If you want median/quantiles inside the violin
  # stat_summary(fun = "median", geom = "point", size = 2, color = "red") +
  theme_minimal() +
  labs(x = "Site", y = "Coral cover", title = "Coral cover across sites") +
  theme(
    axis.text.x = element_text(size = 14, angle = 45, hjust = 1),  
    axis.text.y = element_text(size = 14),                         
    axis.title.x = element_text(size = 16, face = "bold"),         
    axis.title.y = element_text(size = 16, face = "bold"),         
    legend.title = element_text(size = 14, face = "bold"),         
    legend.text  = element_text(size = 12),                        
    plot.title   = element_text(size = 18, hjust = 0.5),           
    plot.margin = margin(t = 20, r = 20, b = 30, l = 40, unit = "pt")
  )



#### a) ANOVA (parametric) ----
# H0:  means of the groups are equal
anova_result <- aov(tCORAL ~ factor(survey_name), data = dat)
summary(anova_result)
# Df  Sum Sq  Mean Sq F value Pr(>F)  
# factor(survey_name)  8 0.04017 0.005021   2.404 0.0298 *
#   Residuals           45 0.09399 0.002089                 
# ---
#   Signif. codes:  0 ‘***’ 0.001 ‘**’ 0.01 ‘*’ 0.05 ‘.’ 0.1 ‘ ’ 1

### interpretation
# Since p_val < 0.05, we reject the null hypothesis (H0) at the 5% significance level.
# So coral cover (tCORAL) differs significantly across at least some of the surveyed sites. 
# However, ANOVA does not tell you which specific sites differ — 
# for that, you need a post-hoc test such as Tukey’s HSD.


# Assumptions of ANOVA
# 
# 1) Independence of observations
# The data from each group (site) should be independent.
# This is mainly ensured by study design.
# 
# 2) Normality of residuals
# The residuals from the model should be approximately normally distributed.
# 
shapiro.test(residuals(anova_result))
# if p_val < 0.05 the assumption of normality is not respected... --> Non-parametric alternative

x11()
qqnorm(residuals(anova_result))
qqline(residuals(anova_result), col = "red")


# 3) Homogeneity of variances (homoscedasticity)
# The variance of tCORAL should be roughly equal across sites.

leveneTest(tCORAL ~ factor(survey_name), data = dat)
# or Bartlett’s test:
bartlett.test(tCORAL ~ factor(survey_name), data = dat)
# If p > 0.05, homogeneity holds.


############# if and only if the assumptions holds (not our case) and the ANOVA is significant...
# Run Tukey's Honest Significant Difference (HSD) test
tukey_result <- TukeyHSD(anova_result)

# Print results
print(tukey_result)

# Optional: plot confidence intervals for pairwise comparisons
x11()
plot(tukey_result, las = 1, col = "blue")




#### b) Non-parametric alternative ----
kruskal.test(tCORAL ~ factor(survey_name), data = dat)
# Kruskal-Wallis rank sum test
# 
# data:  tCORAL by factor(survey_name)
# Kruskal-Wallis chi-squared = 19.076, df = 8, p-value = 0.01446

### interpretation
# p < 0.05, you reject the null hypothesis (H₀) that all groups (survey sites) have the same distribution of coral cover (tCORAL).
# at least one site shows a significantly different distribution of coral cover compared to others.

# The Kruskal–Wallis test is non-parametric, so it does not assume normality. 
# However, it doesn’t tell you which groups differ (same as ANOVA)
# for that, you’d need a post-hoc test (e.g., Dunn’s test with p-value adjustment).

# Dunn test with Bonferroni correction
dunn.test(dat$tCORAL, dat$survey_name, method = "bonferroni")




######  ~> coral vs macroalgae  ----
###### ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Is coral cover correlated with macroalgae?

# Correlation between coral and macroalgae
cor.test(dat$tCORAL, dat$tMA + dat$tFMA, method = "spearman") 
# Spearman's rank correlation rho
# 
# data:  dat$tCORAL and dat$tMA + dat$tFMA
# S = 24464, p-value = 0.6276
# alternative hypothesis: true rho is not equal to 0
# sample estimates:
#        rho 
# 0.06751135 

### interpretation
# very weak and not statistically significant.
# Correlation coefficient (rho): 0.068 
# This indicates an extremely weak positive relationship between coral and macroalgae cover, close to zero!!
# it goes from 1 strong positive relationship to -1 strong negative one
# p-value: 0.628 
# Much greater than the usual significance threshold (e.g., 0.05), 
# we cannot reject the null hypothesis. There is no evidence of a real correlation between coral cover and macroalgae cover
# Conclusion: Coral cover and macroalgae cover do not show a meaningful association.

# let's plot it, I like visual representations =)
x11()
ggplot(dat, aes(x = tMA + tFMA, y = tCORAL)) +
  geom_point(color = "darkgreen") +
  geom_smooth(method = "lm", se = TRUE, color = "black") +
  theme_minimal() +
  labs(x = "Macroalgae cover", y = "Coral cover", 
       title = "Relationship between macroalgae and coral cover")
