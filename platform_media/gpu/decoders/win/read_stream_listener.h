// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef READ_STREAM_LISTENER_H
#define READ_STREAM_LISTENER_H

namespace content {

class ReadStreamListener {
public:
  virtual void OnReadData(int size) = 0;
};

}

#endif // READ_STREAM_LISTENER_H
