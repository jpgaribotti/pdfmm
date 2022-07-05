/**
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Lesser General Public License 2.1.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <pdfmm/private/PdfDeclarationsPrivate.h>
#include "PdfCanvasInputDevice.h"
#include "PdfCanvas.h"
#include "PdfStreamDevice.h"

using namespace std;
using namespace mm;

PdfCanvasInputDevice::PdfCanvasInputDevice(const PdfCanvas& canvas)
    : m_deviceSwitchOccurred(false)
{
    auto contents = canvas.GetContentsObject();
    if (contents != nullptr)
    {
        if (contents->IsArray())
        {
            auto& contentsArr = contents->GetArray();
            for (unsigned i = 0; i < contentsArr.GetSize(); i++)
            {
                auto& streamObj = contentsArr.FindAt(i);
                m_contents.push_back(&streamObj);
            }
        }
        else if (contents->IsDictionary())
        {
            // NOTE: Pages are allowed to be empty
            if (contents->HasStream())
                m_contents.push_back(contents);
        }
        else
        {
            PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidDataType, "Page /Contents not stream or array of streams");
        }
    }

    if (m_contents.size() == 0)
    {
        m_eof = true;
    }
    else
    {
        if (tryPopNextDevice())
            m_eof = m_device->Eof();
        else
            m_eof = false;
    }
}

bool PdfCanvasInputDevice::peek(char& ch) const
{
    if (m_eof)
    {
        ch = '\0';
        return false;
    }

    InputStreamDevice* device = nullptr;
    auto& mref = const_cast<PdfCanvasInputDevice&>(*this);
    while (true)
    {
        if (!mref.tryGetNextDevice(device))
        {
            mref.setEOF();
            ch = '\0';
            return false;
        }

        int ret = device->Peek();
        if (ret != -1)
        {
            if (m_deviceSwitchOccurred)
            {
                // Handle device switch by returning
                // a newline separator. NOTE: Don't
                // reset the switch flag
                ch = '\n';
                return true;
            }

            ch = (char)ret;
            return true;
        }
    }
}

size_t PdfCanvasInputDevice::readBuffer(char* buffer, size_t size, bool& eof)
{
    size_t ret = readBuffer(buffer, size);
    eof = m_eof;
    return ret;
}

size_t PdfCanvasInputDevice::readBuffer(char* buffer, size_t size)
{
    if (size == 0 || m_eof)
        return 0;

    size_t readLocal;
    size_t readCount = 0;
    InputStreamDevice* device = nullptr;
    while (true)
    {
        if (!tryGetNextDevice(device))
        {
            setEOF();
            return readCount;
        }

        if (m_deviceSwitchOccurred)
        {
            if (device->Peek() == -1)
                continue;

            // Handle device switch by inserting
            // a newline separator in the buffer
            // and reset the flag
            *(buffer + readCount) = '\n';
            size -= 1;
            readCount += 1;
            m_deviceSwitchOccurred = false;
            if (size == 0)
                return readCount;
        }

        // Span reads into multple input devices
        // FIX-ME: We should eagerly check if the device(s)
        // reached EOF before retuning
        readLocal = device->Read(buffer + readCount, size);
        size -= readLocal;
        readCount += readLocal;
        if (size == 0)
            return readCount;
    }
}

bool PdfCanvasInputDevice::readChar(char& ch)
{
    if (m_eof)
    {
        ch = '\0';
        return false;
    }

    InputStreamDevice* device = nullptr;
    while (true)
    {
        if (!tryGetNextDevice(device))
        {
            setEOF();
            return false;
        }

        if (m_deviceSwitchOccurred)
        {
            if (device->Peek() == -1)
                continue;

            // Handle device switch by returning a
            // newline separator and reset the flag
            ch = '\n';
            m_deviceSwitchOccurred = false;
            return true;
        }

        if (device->Read(ch))
            return true;
    }
}

size_t PdfCanvasInputDevice::GetLength() const
{
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::NotImplemented, "Unsupported");
}

size_t PdfCanvasInputDevice::GetPosition() const
{
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::NotImplemented, "Unsupported");
}

bool PdfCanvasInputDevice::tryGetNextDevice(InputStreamDevice*& device)
{
    PDFMM_ASSERT(m_device != nullptr);
    if (device == nullptr)
    {
        device = m_device.get();
        return true;
    }

    if (m_contents.size() == 0)
    {
        device = nullptr;
        return false;
    }

    if (!tryPopNextDevice())
    {
        device = nullptr;
        return false;
    }

    // ISO 32000-1:2008: Table 30 – Entries in a page object,
    // /Contents: "The division between streams may occur
    // only at the boundaries between lexical tokens".
    // We will handle the device switch by addind a
    // newline separator
    m_deviceSwitchOccurred = true;
    device = m_device.get();
    return true;
}

bool PdfCanvasInputDevice::tryPopNextDevice()
{
    auto contents = m_contents.front()->GetStream();
    m_contents.pop_front();
    if (contents == nullptr)
    {
        m_device = nullptr;
        return false;
    }
    else
    {
        contents->ExtractTo(m_buffer);
        m_device = std::make_unique<SpanStreamDevice>(m_buffer);
        return true;
    }
}

void PdfCanvasInputDevice::setEOF()
{
    m_deviceSwitchOccurred = false;
    m_eof = true;
}
