#
# Autogenerated by Thrift
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#  @generated
#

from thrift.py3.server cimport ServiceInterface


cdef class MyServiceInterface(ServiceInterface):
    cdef public bint _pass_context_ping
    cdef public bint _pass_context_getRandomData
    cdef public bint _pass_context_hasDataById
    cdef public bint _pass_context_getDataById
    cdef public bint _pass_context_putDataById
    cdef public bint _pass_context_lobDataById
    cdef public bint _pass_context_doNothing
    pass

cdef class MyServicePrioParentInterface(ServiceInterface):
    cdef public bint _pass_context_ping
    cdef public bint _pass_context_pong
    pass

cdef class MyServicePrioChildInterface(MyServicePrioParentInterface):
    cdef public bint _pass_context_pang
    pass

