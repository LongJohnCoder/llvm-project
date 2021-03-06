//===--- RedundantPreprocessorCheck.cpp - clang-tidy ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RedundantPreprocessorCheck.h"
#include "clang/Frontend/CompilerInstance.h"

namespace clang {
namespace tidy {
namespace readability {

namespace {
/// Information about an opening preprocessor directive.
struct PreprocessorEntry {
  SourceLocation Loc;
  /// Condition used after the preprocessor directive.
  std::string Condition;
};

class RedundantPreprocessorCallbacks : public PPCallbacks {
  enum DirectiveKind { DK_If = 0, DK_Ifdef = 1, DK_Ifndef = 2 };

public:
  explicit RedundantPreprocessorCallbacks(ClangTidyCheck &Check,
                                          Preprocessor &PP)
      : Check(Check), PP(PP),
        WarningDescription("nested redundant %select{#if|#ifdef|#ifndef}0; "
                           "consider removing it"),
        NoteDescription("previous %select{#if|#ifdef|#ifndef}0 was here") {}

  void If(SourceLocation Loc, SourceRange ConditionRange,
          ConditionValueKind ConditionValue) override {
    StringRef Condition =
        Lexer::getSourceText(CharSourceRange::getTokenRange(ConditionRange),
                             PP.getSourceManager(), PP.getLangOpts());
    CheckMacroRedundancy(Loc, Condition, IfStack, DK_If, DK_If, true);
  }

  void Ifdef(SourceLocation Loc, const Token &MacroNameTok,
             const MacroDefinition &MacroDefinition) override {
    std::string MacroName = PP.getSpelling(MacroNameTok);
    CheckMacroRedundancy(Loc, MacroName, IfdefStack, DK_Ifdef, DK_Ifdef, true);
    CheckMacroRedundancy(Loc, MacroName, IfndefStack, DK_Ifdef, DK_Ifndef,
                         false);
  }

  void Ifndef(SourceLocation Loc, const Token &MacroNameTok,
              const MacroDefinition &MacroDefinition) override {
    std::string MacroName = PP.getSpelling(MacroNameTok);
    CheckMacroRedundancy(Loc, MacroName, IfndefStack, DK_Ifndef, DK_Ifndef,
                         true);
    CheckMacroRedundancy(Loc, MacroName, IfdefStack, DK_Ifndef, DK_Ifdef,
                         false);
  }

  void Endif(SourceLocation Loc, SourceLocation IfLoc) override {
    if (!IfStack.empty() && IfLoc == IfStack.back().Loc)
      IfStack.pop_back();
    if (!IfdefStack.empty() && IfLoc == IfdefStack.back().Loc)
      IfdefStack.pop_back();
    if (!IfndefStack.empty() && IfLoc == IfndefStack.back().Loc)
      IfndefStack.pop_back();
  }

private:
  void CheckMacroRedundancy(SourceLocation Loc, StringRef MacroName,
                            SmallVector<PreprocessorEntry, 4> &Stack,
                            DirectiveKind WarningKind, DirectiveKind NoteKind,
                            bool Store) {
    if (PP.getSourceManager().isInMainFile(Loc)) {
      for (const auto &Entry : Stack) {
        if (Entry.Condition == MacroName) {
          Check.diag(Loc, WarningDescription) << WarningKind;
          Check.diag(Entry.Loc, NoteDescription, DiagnosticIDs::Note)
              << NoteKind;
        }
      }
    }

    if (Store)
      // This is an actual directive to be remembered.
      Stack.push_back({Loc, MacroName});
  }

  ClangTidyCheck &Check;
  Preprocessor &PP;
  SmallVector<PreprocessorEntry, 4> IfStack;
  SmallVector<PreprocessorEntry, 4> IfdefStack;
  SmallVector<PreprocessorEntry, 4> IfndefStack;
  const std::string WarningDescription;
  const std::string NoteDescription;
};
} // namespace

void RedundantPreprocessorCheck::registerPPCallbacks(
    CompilerInstance &Compiler) {
  Compiler.getPreprocessor().addPPCallbacks(
      ::llvm::make_unique<RedundantPreprocessorCallbacks>(
          *this, Compiler.getPreprocessor()));
}

} // namespace readability
} // namespace tidy
} // namespace clang
