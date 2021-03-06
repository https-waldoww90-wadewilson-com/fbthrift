#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import copy
import math
import unittest

from testing.types import (
    Color,
    Integers,
    Optionals,
    Reserved,
    Runtime,
    SlowCompare,
    easy,
    hard,
    mixed,
    numerical,
)


class StructTests(unittest.TestCase):
    def test_copy(self) -> None:
        x = easy(val=1, an_int=Integers(small=300), name="foo", val_list=[1, 2, 3, 4])
        dif_list = copy.copy(x.val_list)
        self.assertEqual(x.val_list, dif_list)
        dif_int = copy.copy(x.an_int)
        self.assertEqual(x.an_int, dif_int)

    def test_hashability(self) -> None:
        hash(easy())

    def test_optional_struct_creation(self) -> None:
        with self.assertRaises(TypeError):
            easy(1, [1, 1], "test", Integers(tiny=1))  # type: ignore
        easy(val=1, an_int=Integers(small=500))
        with self.assertRaises(TypeError):
            easy(name=b"binary")  # type: ignore
        # Only Required Fields don't accept None
        easy(val=5, an_int=None)

    def test_required_fields(self) -> None:
        with self.assertRaises(TypeError):
            # None is not acceptable as a string
            hard(  # type: ignore
                val=1, val_list=[1, 2], name=None, an_int=Integers(small=1)
            )

        with self.assertRaises(TypeError):
            hard(val=1, val_list=[1, 2])  # type: ignore

    def test_call_replace(self) -> None:
        x = easy(val=1, an_int=Integers(small=300), name="foo")
        y = x(name="bar")
        self.assertNotEqual(x.name, y.name)
        z = y(an_int=None, val=4)
        self.assertNotEqual(x.an_int, z.an_int)
        self.assertNotEqual(x.val, z.val)
        self.assertIsNone(z.an_int.value)
        self.assertEqual(y.val, x.val)
        self.assertEqual(y.an_int, x.an_int)
        x = easy()
        self.assertIsNotNone(x.val)
        self.assertIsNotNone(x.val_list)
        self.assertIsNone(x.name)
        self.assertIsNotNone(x.an_int)

    def test_call_replace_container(self) -> None:
        x = Optionals(values=["a", "b", "c"])
        z = x(values=["b", "c"])
        self.assertEqual(z.values, ["b", "c"])
        y = z(values=None)
        self.assertIsNone(y.values)

    def test_call_replace_required(self) -> None:
        x = hard(
            val=5,
            val_list=[1, 2],
            name="something",
            an_int=Integers(small=1),
            other="non default",
        )
        y = x(other=None)
        self.assertEqual(y.other, "some default")
        with self.assertRaises(TypeError):
            x(name=None)  # type: ignore

    def test_required_with_defaults(self) -> None:
        re = easy(val=10)
        x = mixed(req_easy_ref=re)
        self.assertEqual(x.opt_field, "optional")
        self.assertEqual(x.req_field, "required")
        self.assertEqual(x.unq_field, "unqualified")
        self.assertTrue(x)
        y = mixed(req_field="foo", opt_field="bar", unq_field="baz", req_easy_ref=re)
        self.assertTrue(x)
        z = y(req_field=None, opt_field=None, unq_field=None)
        self.assertEqual(x, z)
        self.assertTrue(x)
        self.assertIsNone(x.opt_easy_ref)
        self.assertIsNone(y.opt_easy_ref)
        self.assertIsNone(z.opt_easy_ref)
        e = easy(val=5)
        z = x(opt_easy_ref=e, req_easy_ref=re)
        self.assertEqual(z.opt_easy_ref, e)
        self.assertNotEqual(x, y)
        y = z(opt_easy_ref=None)
        self.assertIsNone(y.opt_easy_ref)
        self.assertEqual(y, x)
        with self.assertRaises(TypeError):
            z(req_easy_ref=None)  # type: ignore

    def test_runtime_checks(self) -> None:
        x = Runtime()
        with self.assertRaises(TypeError):
            x(bool_val=5)  # type: ignore

        with self.assertRaises(TypeError):
            Runtime(bool_val=5)  # type: ignore

        with self.assertRaises(TypeError):
            x(enum_val=2)  # type: ignore

        with self.assertRaises(TypeError):
            Runtime(enum_val=2)  # type: ignore

        with self.assertRaises(TypeError):
            x(int_list_val=["foo", "bar", "baz"])  # type: ignore

        with self.assertRaises(TypeError):
            Runtime(int_list_val=["foo", "bar", "baz"])  # type: ignore

    def test_reserved(self) -> None:
        x = Reserved(from_="hello", nonlocal_=3, ok="bye", is_cpdef=True)
        self.assertEqual(x.from_, "hello")
        self.assertEqual(x.nonlocal_, 3)
        self.assertEqual(x.ok, "bye")
        self.assertEqual(x.is_cpdef, True)

    def test_ordering(self) -> None:
        x = Runtime(bool_val=False, enum_val=Color.red, int_list_val=[64, 128])
        y = x(bool_val=True)
        self.assertLess(x, y)
        self.assertLessEqual(x, y)
        self.assertGreater(y, x)
        self.assertGreaterEqual(y, x)
        self.assertEquals([x, y], sorted([y, x]))

    def test_noncomparable(self) -> None:
        x = SlowCompare(field1="text", field2=10, field3=Color.red)
        y = x(field3=Color.blue)
        x2 = SlowCompare(field1="text", field2=10, field3=Color.red)

        self.assertEqual(x, x2)
        self.assertNotEqual(x, y)


class NumericalConversionsTests(unittest.TestCase):
    def test_overflow(self) -> None:
        with self.assertRaises(OverflowError):
            numerical(req_float_val=5, req_int_val=2 ** 63 - 1)

        with self.assertRaises(OverflowError):
            numerical(req_float_val=5, req_int_val=2, int_list=[5, 2 ** 32])

    def test_int_to_float(self) -> None:
        x = numerical(req_int_val=5, req_float_val=5, float_val=5, float_list=[1, 5, 6])
        x(req_float_val=10)
        x(float_val=10)
        x(float_list=[6, 7, 8])

    def test_int_to_i64(self) -> None:
        large = 2 ** 63 - 1
        numerical(req_int_val=5, req_float_val=5, i64_val=int(large))
        too_large = 2 ** 65 - 1
        with self.assertRaises(OverflowError):
            numerical(req_int_val=5, req_float_val=5, i64_val=int(too_large))

    def test_float_to_int_required_field(self) -> None:
        with self.assertRaises(TypeError):
            numerical(req_int_val=math.pi, req_float_val=math.pi)  # type: ignore

    def test_float_to_int_unqualified_field(self) -> None:
        with self.assertRaises(TypeError):
            numerical(  # type: ignore
                req_int_val=5, req_float_val=math.pi, int_val=math.pi
            )

    def test_float_to_int_list(self) -> None:
        with self.assertRaises(TypeError):
            numerical(
                req_int_val=5,
                req_float_val=math.pi,
                int_list=[math.pi, math.e],  # type: ignore
            )
