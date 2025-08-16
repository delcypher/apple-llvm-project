//==--- DiagnosticCodeGenKinds.td - CodeGen Diagnostics -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 
//===----------------------------------------------------------------------===//
let Component = "CodeGen" in {
let CategoryName = "UBSan" in {

def trap_ubsan_add : Trap<"%select{signed|unsigned}0 integer addition overflow on type %1">;

}
}
