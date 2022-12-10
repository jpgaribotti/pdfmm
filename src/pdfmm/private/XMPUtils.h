/**
 * SPDX-FileCopyrightText: (C) 2022 Francesco Pretto <ceztko@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef PDFMM_PDFA_FUNCTIONS_H
#define PDFMM_PDFA_FUNCTIONS_H

#include <pdfmm/base/PdfXMPMetadata.h>
#include <pdfmm/base/PdfXMPPacket.h>

namespace mm
{
    PdfXMPMetadata GetXMPMetadata(const std::string_view& xmpview, std::unique_ptr<PdfXMPPacket>& packet);
    void UpdateOrCreateXMPMetadata(std::unique_ptr<PdfXMPPacket>& packet, const PdfXMPMetadata& metatata);
}

#endif // PDFMM_PDFA_FUNCTIONS_H
