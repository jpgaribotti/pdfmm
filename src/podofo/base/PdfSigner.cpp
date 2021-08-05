/**
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Lesser General Public License 2.1 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include "PdfSigner.h"
#include "../base/PdfDictionary.h"
#include "base/PdfOutputDevice.h"

using namespace std;
using namespace PoDoFo;

constexpr const char* ByteRangeBeacon = "[ 0 1234567890 1234567890 1234567890]";
constexpr size_t BufferSize = 65536;

static size_t ReadForSignature(PdfOutputDevice& device,
    size_t conentsBeaconOffset, size_t conentsBeaconSize,
    char* buffer, size_t len);
static void AdjustByteRange(PdfOutputDevice& device, size_t byteRangeOffset,
    size_t conentsBeaconOffset, size_t conentsBeaconSize);
static void SetSignature(PdfOutputDevice& device, const string_view& sigData,
    size_t conentsBeaconOffset);
static void PrepareBeaconsData(size_t signatureSize, string& contentsBeacon, string& byteRangeBeacon);

PdfSigner::~PdfSigner() { }

string PdfSigner::GetSignatureFilter() const
{
    // Default value
    return "Adobe.PPKLite";
}

void PoDoFo::SignDocument(PdfMemDocument& doc, PdfOutputDevice& device, PdfSigner& signer,
    PdfSignature& signature, PdfSignFlags flags)
{
    (void)flags;

    string signatureBuf;
    signer.ComputeSignature(signatureBuf, true);
    size_t beaconSize = signatureBuf.size();
    PdfSignatureBeacons beacons;
    PrepareBeaconsData(beaconSize, beacons.ContentsBeacon, beacons.ByteRangeBeacon);
    signature.PrepareForSigning(signer.GetSignatureFilter(), signer.GetSignatureSubFilter(),
        signer.GetSignatureType(), beacons);
    auto form = doc.GetAcroForm();
    // TABLE 8.68 Signature flags: SignaturesExist (1) | AppendOnly (2)
    form->GetObject().GetDictionary().AddKey("SigFlags", PdfObject((int64_t)3));

    doc.WriteUpdate(device);
    device.Flush();

    AdjustByteRange(device, *beacons.ByteRangeOffset, *beacons.ContentsOffset, beacons.ContentsBeacon.size());
    device.Flush();

    // Read data from the device to prepare the signature
    signer.Reset();
    device.Seek(0);
    buffer_t buffer(BufferSize);
    size_t readBytes;
    while ((readBytes = ReadForSignature(device, *beacons.ContentsOffset, beacons.ContentsBeacon.size(),
        buffer.data(), BufferSize)) != 0)
    {
        signer.AppendData({ buffer.data(), readBytes });
    }

    signer.ComputeSignature(signatureBuf, false);
    if (signatureBuf.size() > beaconSize)
        throw runtime_error("Actual signature size bigger than beacon size");

    // Ensure the signature will be as big as the
    // beacon size previously cached to fill all
    // available reserved space for the /Contents
    signatureBuf.resize(beaconSize);
    SetSignature(device, signatureBuf, *beacons.ContentsOffset);
    device.Flush();
}

size_t ReadForSignature(PdfOutputDevice& device, size_t conentsBeaconOffset, size_t conentsBeaconSize,
    char* buffer, size_t len)
{
    size_t pos = device.Tell();
    size_t numRead = 0;
    // Check if we are before beacon
    if (pos < conentsBeaconOffset)
    {
        size_t readSize = std::min(len, conentsBeaconOffset - pos);
        if (readSize > 0)
        {
            numRead = device.Read(buffer, readSize);
            buffer += numRead;
            len -= numRead;
            if (len == 0)
                return numRead;
        }
    }

    // shift at the end of beacon
    if ((pos + numRead) >= conentsBeaconOffset
        && pos < (conentsBeaconOffset + conentsBeaconSize))
    {
        device.Seek(conentsBeaconOffset + conentsBeaconSize);
    }

    // read after beacon
    len = std::min(len, device.GetLength() - device.Tell());
    if (len == 0)
        return numRead;

    return numRead + device.Read(buffer, len);
}

void AdjustByteRange(PdfOutputDevice& device, size_t byteRangeOffset,
    size_t conentsBeaconOffset, size_t conentsBeaconSize)
{
    // Get final position
    size_t fileEnd = device.GetLength();
    PdfArray arr;
    arr.push_back(PdfObject(static_cast<int64_t>(0)));
    arr.push_back(PdfObject(static_cast<int64_t>(conentsBeaconOffset)));
    arr.push_back(PdfObject(static_cast<int64_t>(conentsBeaconOffset + conentsBeaconSize)));
    arr.push_back(PdfObject(static_cast<int64_t>(fileEnd - (conentsBeaconOffset + conentsBeaconSize))));

    device.Seek(byteRangeOffset);
    arr.Write(device, PdfWriteMode::Compact, nullptr);
}

void SetSignature(PdfOutputDevice& device, const string_view& contentsData,
    size_t conentsBeaconOffset)
{
    auto sig = PdfString::FromRaw(contentsData);

    // Position at contents beacon after '<'
    device.Seek(conentsBeaconOffset);
    // Write the beacon data
    sig.Write(device, PdfWriteMode::Compact, nullptr);
}

void PrepareBeaconsData(size_t signatureSize, string& contentsBeacon, string& byteRangeBeacon)
{
    // Just prepare strings with spaces, for easy writing later
    contentsBeacon.resize((signatureSize * 2) + 2, ' '); // Signature bytes will be encoded
                                                         // as an hex string
    byteRangeBeacon.resize(char_traits<char>::length(ByteRangeBeacon), ' ');
}
