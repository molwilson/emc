# R version 4.4.2


# Day 1 ----

result <- 2+16*24-56
result

output <- "this is the output"

equation <- 2+16*24-56/(2+1)-457
equation


circle <- pi*5^2
circle

brothers_sisters <- c(1, 4, 1, 1, 2, 1)
mean(brothers_sisters)
max(brothers_sisters)


odd_n1 <- seq(from = 1, to = 10, by = 2)
odd_n2 <- seq(from = 1, by = 2, length.out = 5)
odd_n [3]
odd_n[c(2:4)]
odd_n [-4]

x <- c(1:5)
y <- 6
x+y
x*y

(step_1 <- matrix(data = 1:6,
                  nrow = 2,
                  ncol = 3))

# [,1] [,2] [,3]
# [1,]    1    3    5
# [2,]    2    4    6

(step_2 <- matrix(
  data = c("cheetah", "tiger", "ladybug", "deer", "monkey", "crocodile"),
  nrow = 2,
  ncol = 3
))

# [,1]      [,2]      [,3]       
# [1,] "cheetah" "ladybug" "monkey"   
# [2,] "tiger"   "deer"    "crocodile"

num_vector <- c(1,4,3,98,32,-76,-4)
# extract 4th value

num_vector [4]
# [1] 98

num_vector [c(1,3)]
#extract 1st and 3rd values [1] 1 3

num_vector [c(-2, -4)]
#extract all values except second and fourth

num_vector [c(6:10)]

# Day 2 ----

summary_stats <- function(x) {
  mean_x <- mean(x, na.rm = TRUE)
  sd_x <- sd(x, na.rm = TRUE)
  return(c(mean = mean_x, sd = sd_x))
}

summary_stats_list <- function(x) {
  mean_x <- mean(x, na.rm = TRUE)
  sd_x <- sd(x, na.rm = TRUE)
  return(list(mean = mean_x, sd = sd_x))
}

sequence <- seq(from = 1, to = 10, by = 2)
summary_stats(sequence)

pets <- c(1, 3, 5, 1, 7, 2, 7, 0)
summary_stats(pets)

pets_sum <- summary_stats_list(pets)
pets_sum_df <- as.data.frame()


qplot(1:10, 1:10)

evens <- seq(from = 0, to = 10, by = 2)
nums <- c(7, 44, 101, 60, 1)
nums_sorted <- sort(numbers, decreasing = TRUE)

