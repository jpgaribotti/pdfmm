/**
 * Copyright (C) 2016 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <PdfTest.h>

using namespace std;
using namespace mm;

TEST_CASE("testDevices")
{
    string_view testString = "Hello World Buffer!";
    charbuff buffer1;

    // large appends
    PdfStringOutputDevice streamLarge(buffer1);
    for (unsigned i = 0; i < 100; i++)
        streamLarge.Write(testString);

    if (buffer1.size() != testString.size() * 100)
        FAIL(utls::Format("Buffer1 size is wrong after 100 attaches: {}", buffer1.size()));
}
