/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef XMLTokenizer_h
#define XMLTokenizer_h

#if COMPILER(GHS)
#include "XMLToken.h"
#endif
#include "MarkupTokenizerBase.h"

namespace WebCore {

class XMLToken;

class XMLTokenizerState {
public:
    enum State {
        DataState,
        CharacterReferenceStartState,
        EntityReferenceState,
        TagOpenState,
        EndTagOpenState,
        TagNameState,
        EndTagNameState,
        EndTagSpaceState,
        XMLDeclAfterXMLState,
        XMLDeclBeforeVersionNameState,
        XMLDeclAfterVersionNameState,
        XMLDeclBeforeVersionValueState,
        XMLDeclBeforeVersionOnePointState,
        XMLDeclVersionValueQuotedState,
        XMLDeclAfterVersionState,
        XMLDeclBeforeEncodingNameState,
        XMLDeclAfterEncodingNameState,
        XMLDeclBeforeEncodingValueState,
        XMLDeclEncodingValueStartQuotedState,
        XMLDeclEncodingValueQuotedState,
        XMLDeclAfterEncodingState,
        XMLDeclBeforeStandaloneNameState,
        XMLDeclAfterStandaloneNameState,
        XMLDeclBeforeStandaloneValueState,
        XMLDeclStandaloneValueQuotedState,
        XMLDeclAfterStandaloneState,
        XMLDeclCloseState,
        ProcessingInstructionTargetStartState,
        ProcessingInstructionTargetState,
        ProcessingInstructionAfterTargetState,
        ProcessingInstructionDataState,
        ProcessingInstructionCloseState,
        MarkupDeclarationOpenState,
        BeforeAttributeNameState,
        AttributeNameState,
        AfterAttributeNameState,
        BeforeAttributeValueState,
        AttributeValueQuotedState,
        CharacterReferenceInAttributeValueState,
        AfterAttributeValueQuotedState,
        SelfClosingStartTagState,
        CommentState,
        CommentDashState,
        CommentEndState,
        BeforeDOCTYPENameState,
        DOCTYPENameState,
        AfterDOCTYPENameState,
        AfterDOCTYPEPublicKeywordState,
        BeforeDOCTYPEPublicIdentifierState,
        DOCTYPEPublicIdentifierQuotedState,
        AfterDOCTYPEPublicIdentifierState,
        AfterDOCTYPESystemKeywordState,
        BeforeDOCTYPESystemIdentifierState,
        DOCTYPESystemIdentifierQuotedState,
        AfterDOCTYPESystemIdentifierState,
        BeforeDOCTYPEInternalSubsetState,
        AfterDOCTYPEInternalSubsetState,
        CDATASectionState,
    };
};

class XMLTokenizer : public MarkupTokenizerBase<XMLToken, XMLTokenizerState> {
    WTF_MAKE_NONCOPYABLE(XMLTokenizer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<XMLTokenizer> create() { return adoptPtr(new XMLTokenizer); }

    // This function returns true if it emits a token. Otherwise, callers
    // must provide the same (in progress) token on the next call (unless
    // they call reset() first).
    bool nextToken(SegmentedString&, XMLToken&);

    bool errorDuringParsing() const { return m_errorDuringParsing; }

private:
    XMLTokenizer();

    inline void parseError();
    inline void bufferCharacter(UChar);

    bool m_errorDuringParsing;
};

#if COMPILER(GHS)
template<>
QualifiedName AtomicMarkupTokenBase<XMLToken>::nameForAttribute(const XMLToken::Attribute& attribute) const
{
    return QualifiedName(attribute.m_prefix.isEmpty() ? nullAtom : AtomicString(attribute.m_prefix.data(), attribute.m_prefix.size()), AtomicString(attribute.m_name.data(), attribute.m_name.size()), nullAtom);
}
#endif

}

#endif
