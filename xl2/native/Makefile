#******************************************************************************
# Makefile                          (C) 1992-2003 Christophe de Dinechin (ddd) 
#                                                                  XL2 project 
#******************************************************************************
#
#  File Description:
#
#    Makefile for the native version of the XL compiler
#
#
#
#
#
#
#
#
#******************************************************************************
# This program is released under the GNU General Public License.
# See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
#******************************************************************************
#* File       : $RCSFile$
#* Revision   : $Revision$
#* Date       : $Date$
#******************************************************************************

BROOT=../

SOURCES=compiler.xl
OTHER_SOURCES=					\
	xl.scanner.xs				\
	xl.scanner.xl				\
	xl.scanner.position.xs			\
	xl.scanner.position.xl			\
	xl.errors.xs				\
	xl.errors.xl				\
	xl.parser.tree.xs			\
	xl.parser.tree.xl			\
	xl.parser.xs				\
	xl.parser.xl				\
	xl.translator.xs			\
	xl.translator.xl			\
	xl.symbols.xs				\
	xl.symbols.xl				\
	xl.constants.xl				\
	xl.semantics.xs				\
	xl.semantics.xl				\
	xl.semantics.declarations.xs		\
	xl.semantics.declarations.xl		\
	xl.semantics.macros.xs			\
	xl.semantics.macros.xl			\
	xl.semantics.functions.xs		\
	xl.semantics.functions.xl		\
	xl.semantics.iterators.xs		\
	xl.semantics.iterators.xl		\
	xl.semantics.overload.xs		\
	xl.semantics.overload.xl		\
	xl.semantics.generics.xs		\
	xl.semantics.generics.xl		\
	xl.semantics.instructions.xs		\
	xl.semantics.instructions.xl		\
	xl.semantics.modules.xs			\
	xl.semantics.modules.xl			\
	xl.semantics.types.xs			\
	xl.semantics.types.xl			\
	xl.semantics.types.functions.xs		\
	xl.semantics.types.functions.xl		\
	xl.semantics.types.records.xs		\
	xl.semantics.types.records.xl		\
	xl.semantics.types.enumerations.xs	\
	xl.semantics.types.enumerations.xl	\
	xl.semantics.types.generics.xs		\
	xl.semantics.types.generics.xl		\
	xl.semantics.writtenforms.xs		\
	xl.semantics.writtenforms.xl		\
	xl.semantics.pragmas.xl			\
	xl.codegenerator.xs			\
	xl.codegenerator.xl			\
	xl.renderer.xs				\
	xl.renderer.xl				\
	xl.codegenerator.machine.xs		\
	xl.codegenerator.machine.xl		\
	xl.bytecode.xs				\
	xl.bytecode.xl				\
	xl.optimizer.xs				\
	xl.optimizer.xl				\
	xl.textio.xl				\
	../xl_lib.h				\
	../xl.syntax				\
						\
	xl.plugin.xl2c.xl			\
	xl.plugin.listing.xl			\
	xl.plugin.include.xl			\
	xl.plugin.transforms.xl			\
	xl.plugin.differentiation.xl		\
	xl.plugin.constantfold.xl

TOCLEAN=nxl$(EXE)
PRODUCT=nxl$(EXE)
XL=../bootstrap/bxl$(EXE)

include $(BROOT)Makefile.config

qtest: debug regression
otest: opt regression

test: debug regression				\
      native native_test native_regression	\
      native_stability_test
	echo ------ Differences: -----------------------------------------
	diff $(OBJROOT)nnxl_test/native/compiler.xl.C \
	     $(OBJROOT)nnxl_stability/native/compiler.xl.C
	echo ------ End of Differences -----------------------------------

# For now, we rely on mere transcoding to get the next generation compiler
# We disable semantics, which is incomplete enough to cause more harm than good
native:
	$(MAKE) PRODUCT=nnxl$(EXE) XL="./nxl$(EXE)" XLOPTS="-bootstrap -g" OBJROOT="$(OBJROOT)nnxl"
native_test: 
	$(MAKE) PRODUCT=nnnxl$(EXE) XL="./nnxl$(EXE)" XLOPTS="-bootstrap" OBJROOT="$(OBJROOT)nnxl_test"
native_regression:
	cd TESTS; time ./alltests -xl "../nnxl$(EXE)"
native_stability_test: 
	$(MAKE) PRODUCT=nnnnxl$(EXE) XL="./nnnxl$(EXE)" XLOPTS="-bootstrap" OBJROOT="$(OBJROOT)nnxl_stability"
regression: ALWAYS
	cd TESTS; time ./alltests
ALWAYS:

prebuild: xl_lib.h xl.syntax # These are used for the native build
# MinGW's unable to follow symlinks, so we have to copy dependencies
xl_lib.h:
	cp ../xl_lib.h .
xl.syntax:
	cp ../xl.syntax .

.PRECIOUS: $(OBJDIR)/%.xl.C

$(OBJDIR)/%.xl.o: $(OBJDIR)/%.xl.C
	$V $(CXX) -I . $(CXXFLAGS) $(CPPFLAGS_$*) $(CXXFLAGS_$*) -o $@ -c $<

$(OBJDIR)/%.xl.C: %.xl $(XL)
	$V $(XL) $(XLOPTS) $< > $@

$(OBJDIR)/compiler.xl.C :  $(OTHER_SOURCES)
