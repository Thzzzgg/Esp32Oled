// 取樣歷史：固定長度的環狀緩衝區，圖表直接讀它來畫
#pragma once
#include <Arduino.h>
#include <math.h>

#include "config.h"

// 保留最近 HISTORY_LEN 筆取樣；NAN 表示該秒沒有資料（例如斷線）
class History {
 public:
  void push(float value) {
    data_[head_] = value;
    head_ = (head_ + 1) % HISTORY_LEN;
    if (count_ < HISTORY_LEN) count_++;
  }

  size_t size() const { return count_; }

  // i = 0 是最舊的一筆，i = size() - 1 是最新的一筆
  float at(size_t i) const {
    size_t oldest = (head_ + HISTORY_LEN - count_) % HISTORY_LEN;
    return data_[(oldest + i) % HISTORY_LEN];
  }

  float latest() const { return count_ ? at(count_ - 1) : NAN; }

 private:
  float data_[HISTORY_LEN];
  size_t head_ = 0;
  size_t count_ = 0;
};
