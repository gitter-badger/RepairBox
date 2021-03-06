# include "fbcu.bi"




namespace fbc_tests.functions.va_int_and_ptrs

'':::::
sub varints cdecl ( byval n as integer, ... )
	dim va as any ptr
	dim i as integer
	
	va = va_first( )
	
	for i = 1 to n
		CU_ASSERT( va_arg( va, integer ) = i )
		va = va_next( va, integer )
	next
	
end sub

'':::::
sub varintptrs cdecl ( byval n as integer, ... )
	dim va as any ptr
	dim i as integer
	
	va = va_first( )
	
	for i = 1 to n
		CU_ASSERT( *va_arg( va, integer ptr ) )
		va = va_next( va, integer ptr )
	next
	
end sub

''::::
sub vaints_test( d as integer )
	dim as integer a, b, c
	dim as integer ptr pa, pb, pc
	dim as integer ptr ptr ppc
	
	a = 1
	b = 2
	c = 3
	
	pa = @a
	pb = @b
	pc = @c
	ppc = @pc
	
	varints 4, a, *pb, **ppc, d

	varintptrs 4, pa, pb, pc, @d
	
end sub	

sub test_1 cdecl ()

	vaints_test 4

end sub

sub ctor () constructor

	fbcu.add_suite("fbc_tests.functions.va_int_and_ptrs")
	fbcu.add_test("test_1", @test_1)

end sub

end namespace
