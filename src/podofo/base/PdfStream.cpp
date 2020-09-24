/***************************************************************************
 *   Copyright (C) 2005 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   In addition, as a special exception, the copyright holders give       *
 *   permission to link the code of portions of this program with the      *
 *   OpenSSL library under certain conditions as described in each         *
 *   individual source file, and distribute linked combinations            *
 *   including the two.                                                    *
 *   You must obey the GNU General Public License in all respects          *
 *   for all of the code used other than OpenSSL.  If you modify           *
 *   file(s) with this exception, you may extend this exception to your    *
 *   version of the file(s), but you are not obligated to do so.  If you   *
 *   do not wish to do so, delete this exception statement from your       *
 *   version.  If you delete this exception statement from all source      *
 *   files in the program, then also delete it here.                       *
 ***************************************************************************/

#include "PdfStream.h"

#include <doc/PdfDocument.h>
#include "PdfArray.h"
#include "PdfFilter.h"
#include "PdfInputStream.h"
#include "PdfOutputStream.h"
#include "PdfOutputDevice.h"
#include "PdfDefinesPrivate.h"
#include "PdfDictionary.h"

#include <iostream>

#include <stdlib.h>

using namespace std;

namespace PoDoFo {

enum EPdfFilter PdfStream::eDefaultFilter = EPdfFilter::FlateDecode;

PdfStream::PdfStream( PdfObject* pParent )
    : m_pParent( pParent ), m_bAppend( false )
{
}

PdfStream::~PdfStream() { }

void PdfStream::GetFilteredCopy( PdfOutputStream* pStream ) const
{
    TVecFilters vecFilters = PdfFilterFactory::CreateFilterList( m_pParent );
    if( vecFilters.size() )
    {
        auto pDecodeStream = PdfFilterFactory::CreateDecodeStream( vecFilters, *pStream,
            m_pParent ? &m_pParent->GetDictionary() : nullptr );

        pDecodeStream->Write( const_cast<char*>(this->GetInternalBuffer()), this->GetInternalBufferSize() );
        pDecodeStream->Close();
    }
    else
    {
        pStream->Write( const_cast<char*>(this->GetInternalBuffer()), this->GetInternalBufferSize() );
    }
}

void PdfStream::GetFilteredCopy( char** ppBuffer, size_t* lLen ) const
{
    TVecFilters vecFilters = PdfFilterFactory::CreateFilterList( m_pParent );
    PdfMemoryOutputStream  stream;
    if( vecFilters.size() )
    {
        auto pDecodeStream = PdfFilterFactory::CreateDecodeStream( vecFilters, stream,
            m_pParent ? &m_pParent->GetDictionary() : nullptr);

        pDecodeStream->Write( this->GetInternalBuffer(), this->GetInternalBufferSize() );
        pDecodeStream->Close();
    }
    else
    {
        stream.Write( const_cast<char*>(this->GetInternalBuffer()), this->GetInternalBufferSize() );
        stream.Close();
    }

    *lLen = stream.GetLength();
    *ppBuffer = stream.TakeBuffer();
}

const PdfStream & PdfStream::operator=(const PdfStream & rhs)
{
    if (&rhs == this)
        return *this;

    CopyFrom(rhs);
    return (*this);
}

void PdfStream::CopyFrom(const PdfStream &rhs)
{
    PdfMemoryInputStream stream( rhs.GetInternalBuffer(), rhs.GetInternalBufferSize() );
    this->SetRawData( stream );

    if( m_pParent ) 
        m_pParent->GetDictionary().AddKey( PdfName::KeyLength, 
                                           PdfVariant(static_cast<int64_t>(rhs.GetInternalBufferSize())));
}

void PdfStream::Set( const char* szBuffer, size_t lLen, const TVecFilters & vecFilters )
{
    this->BeginAppend( vecFilters );
    this->Append( szBuffer, lLen );
    this->EndAppend();
}

void PdfStream::Set( const char* szBuffer, size_t lLen )
{
    this->BeginAppend();
    this->Append( szBuffer, lLen );
    this->EndAppend();
}

void PdfStream::Set( PdfInputStream* pStream )
{
    TVecFilters vecFilters;

    if( eDefaultFilter != EPdfFilter::None )
        vecFilters.push_back( eDefaultFilter );

    this->Set( pStream, vecFilters );
}

void PdfStream::Set( PdfInputStream* pStream, const TVecFilters & vecFilters )
{
    const int BUFFER_SIZE = 4096;
    size_t lLen = 0;
    char buffer[BUFFER_SIZE];

    this->BeginAppend( vecFilters );

    do {
        lLen = pStream->Read( buffer, BUFFER_SIZE );
        this->Append( buffer, lLen );
    } while( lLen == BUFFER_SIZE );

    this->EndAppend();
}

void PdfStream::SetRawData( PdfInputStream &pStream, ssize_t lLen )
{
    const size_t   BUFFER_SIZE = 4096;
    char           buffer[BUFFER_SIZE];
    size_t         lRead;
    TVecFilters    vecEmpty;

    // TODO: DS, give begin append a size hint so that it knows
    //       how many data has to be allocated
    this->BeginAppend( vecEmpty, true, false );
    if( lLen < 0 ) 
    {
        do {
            lRead = pStream.Read( buffer, BUFFER_SIZE );
            this->Append( buffer, lRead );
        } while( lRead > 0 );
    }
    else
    {
        size_t sizeLeft;
        do
        {
            lRead = pStream.Read( buffer, std::min( BUFFER_SIZE, (size_t)lLen ), &sizeLeft);
            lLen -= lRead;
            this->Append( buffer, lRead );
        } while( lLen && lRead > 0 );
    }

    this->EndAppend();
}

void PdfStream::BeginAppend( bool bClearExisting )
{
    TVecFilters vecFilters;

    if( eDefaultFilter != EPdfFilter::None )
        vecFilters.push_back( eDefaultFilter );

    this->BeginAppend( vecFilters, bClearExisting );
}

void PdfStream::BeginAppend( const TVecFilters & vecFilters, bool bClearExisting, bool bDeleteFilters )
{
    char* pBuffer = NULL;
    size_t lLen = 0; //RG: TODO Should this variable be initialised with 0 (line 225 may fall through without initialisation!)

    PODOFO_RAISE_LOGIC_IF( m_bAppend, "BeginAppend() failed because EndAppend() was not yet called!" );

    if( m_pParent )
    {
        // We must make sure the parent will be set dirty. All methods
        // writing to the stream will call this method first
        m_pParent->SetDirty();

        auto document = m_pParent->GetDocument();
        if (document != nullptr)
            document->GetObjects().BeginAppendStream( this );
    }

    if( !bClearExisting && this->GetLength() ) 
        this->GetFilteredCopy( &pBuffer, &lLen );

    if ( m_pParent )
    {
        if( vecFilters.size() == 0 )
        {
            if ( bDeleteFilters )
                m_pParent->GetDictionary().RemoveKey( PdfName::KeyFilter );
        }
        else if ( vecFilters.size() == 1 )
        {
            m_pParent->GetDictionary().AddKey( PdfName::KeyFilter, 
                                               PdfName( PdfFilterFactory::FilterTypeToName( vecFilters.front() ) ) );
        }
        else // vecFilters.size() > 1
        {
            PdfArray filters;
            TCIVecFilters it = vecFilters.begin();
            while( it != vecFilters.end() )
            {
                filters.push_back( PdfName( PdfFilterFactory::FilterTypeToName( *it ) ) );
                ++it;
            }
        
            m_pParent->GetDictionary().AddKey( PdfName::KeyFilter, filters );
        }
    }

    this->BeginAppendImpl( vecFilters );
    m_bAppend = true;
    if( pBuffer ) 
    {
        this->Append( pBuffer, lLen );
        podofo_free( pBuffer );
    }
}

void PdfStream::EndAppend()
{
    PODOFO_RAISE_LOGIC_IF( !m_bAppend, "EndAppend() failed because BeginAppend() was not yet called!" );

    m_bAppend = false;
    this->EndAppendImpl();

    PdfDocument* document;
    if( m_pParent && (document = m_pParent->GetDocument()) != nullptr )
        document->GetObjects().EndAppendStream( this );
}

// -----------------------------------------------------
//
// -----------------------------------------------------
void PdfStream::Set( const char* pszString )
{
    if( pszString )
        Set( const_cast<char*>(pszString), strlen( pszString ) );
}

void PdfStream::Append( const char* pszString, size_t lLen )
{
    PODOFO_RAISE_LOGIC_IF( !m_bAppend, "Append() failed because BeginAppend() was not yet called!" );

    this->AppendImpl( pszString, lLen );
}

void PdfStream::Append( const char* pszString )
{
    if( pszString )
        Append( pszString, strlen( pszString ) );
}

void PdfStream::Append( const std::string& sString )
{
    Append( sString.c_str(), sString.length() );
}

bool PdfStream::IsAppending() const
{
    return m_bAppend;
}

};
