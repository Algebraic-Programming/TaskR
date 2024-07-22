/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file dispatcher.cpp
 * @brief Unit tests for the dispatcher class
 * @author S. M. Martin
 * @date 11/9/2023
 */

#include "gtest/gtest.h"
#include <taskr/dispatcher.hpp>
#include <taskr/tasking.hpp>
#include <taskr/task.hpp>

TEST(Dispatcher, Construction)
{
  // Creating by context
  EXPECT_NO_THROW(HiCR::tasking::Dispatcher([]() { return (HiCR::tasking::Task *)NULL; }));

  // Creating by new
  HiCR::tasking::Dispatcher *d = NULL;
  EXPECT_NO_THROW(d = new HiCR::tasking::Dispatcher([]() { return (HiCR::tasking::Task *)NULL; }));
  delete d;
}

TEST(Dispatcher, Pull)
{
  HiCR::tasking::Dispatcher d([]() { return (HiCR::tasking::Task *)NULL; });
  HiCR::tasking::Task      *t = NULL;
  EXPECT_NO_THROW(t = d.pull());
  EXPECT_EQ(t, (HiCR::tasking::Task *)NULL);
}
