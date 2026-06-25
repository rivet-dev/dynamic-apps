! gtk-source-lang: fortran
program test95
  ! Comment: This is a Fortran free-form program
!$ use omp_lib
  ! The previous line is not a comment, but an OpenMP directive
  implicit none
  character(len=9),parameter  :: t='Some text'
  integer,parameter           :: a=1
  real,parameter              :: r1=1.23e0, r2=.12, r3=12., r4=12.12, r5=1.d0
!$  real*8                    :: t1, t2
  integer                     :: i

!$ t1 = omp_get_wtime()
  if ( .true. ) then
    print *, a, t
    print *,size( (/ r1, r2, r3, r4, r5 /) )
  endif
  ! parallel do
  do i=1,100
    print *,i
  enddo ! i
  ! end parallel do
!$ t2 = omp_get_wtime()
!$ print *,'The loop took ',t2-t1,'seconds'
end program
