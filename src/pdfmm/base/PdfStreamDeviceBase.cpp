/**
 * SPDX-FileCopyrightText: (C) 2006 Dominik Seichter <domseichter@web.de>
 * SPDX-FileCopyrightText: (C) 2020 Francesco Pretto <ceztko@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <pdfmm/private/PdfDeclarationsPrivate.h>
#include "PdfStreamDeviceBase.h"

using namespace std;
using namespace mm;

static string_view getAccessString(DeviceAccess access);

StreamDeviceBase::StreamDeviceBase()
    : m_Access{ }
{
}

void StreamDeviceBase::Seek(size_t offset)
{
    if (!CanSeek())
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidDeviceOperation, "Tried to seek an unseekable input device");

    seek((ssize_t)offset, SeekDirection::Begin);
}

void StreamDeviceBase::Seek(ssize_t offset, SeekDirection direction)
{
    if (!CanSeek())
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidDeviceOperation, "Tried to seek an unseekable input device");

    seek(offset, direction);
}

void StreamDeviceBase::Close()
{
    close();
}

bool StreamDeviceBase::CanSeek() const
{
    return false;
}

void StreamDeviceBase::EnsureAccess(DeviceAccess access) const
{
    if ((m_Access & access) == DeviceAccess{ })
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "Mismatch access for this device, requested {}", getAccessString(access));
}

void StreamDeviceBase::seek(ssize_t offset, SeekDirection direction)
{
    (void)offset;
    (void)direction;
    PDFMM_RAISE_ERROR(PdfErrorCode::NotImplemented);
}

void StreamDeviceBase::close()
{
    // Do nothing
}

string_view getAccessString(DeviceAccess access)
{
    switch (access)
    {
        case DeviceAccess::Read:
            return "Read"sv;
        case DeviceAccess::Write:
            return "Write"sv;
        default:
            PDFMM_RAISE_ERROR(PdfErrorCode::InvalidEnumValue);
    }
}
