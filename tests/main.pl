#!/usr/bin/perl

use strict;
use warnings;

use IO::Socket::INET;

my $n_tests = 0;
my $n_ok = 0;
my $n_fail = 0;

my $sock = IO::Socket::INET->new("127.0.0.1:25");
$sock->autoflush(1);

sub check_re {
	my ($input, $data, $re, $name) = @_;
	++$n_tests;
	if ($data =~ $re) {
		print "[  OK  ] Test #$n_tests, $name\n";
		++$n_ok;
	} else {
		print "[ FAIL ] Test #$n_tests, $name\n";
		print "         Sent: $input\n";
		print "         Expected: /$re/\n";
		print "         Found: $data\n";
		++$n_fail;
	}
}

sub print_stat {
	print "[ DONE ] $n_ok tests passed, $n_fail tests failed; $n_tests tests total\n";
}

sub test {
	my ($input, $re, $name) = @_;
	$sock->print("$input\r\n");

	my @lines;
	my $max_lines = 10;
	my $done = 0;
	do {
		my $line = $sock->getline;
		chomp $line;
		push @lines, $line;
		$done = 1 unless $line =~ /^\d+-/;
	} while (!$done);

	my $line_no = 1;
	for (@lines) {
		check_re($input, $_, $re, $name. (scalar(@lines) > 1 ? ", line $line_no" : ""));
		++$line_no;
	}
}

my $greet = $sock->getline;
chomp $greet;
check_re("", $greet, q/220.*Ready/, "Welcome message");

test("HELO", q/500 Syntax error/, "Invalid HELO message");
test("EHLO", q/500 Syntax error/, "Invalid EHLO message");
test("HELO test", q/250 /, "Valid HELO message");
test("EHLO test", q/500 Unknown command/, "Valid EHLO message, but HELO done");

my @invalid_emails = qw( test@mail @mail @test@mail.ru @mail:test@mail.ru test@ @ );
my @valid_emails = qw( test0@mail.ru @test.ru:test1@mail.ru );

for (@invalid_emails) {
	test("MAIL FROM:<$_>", q/500 Syntax error/, "Invalid MAIL email: $_");
}

my $i = 0;
for (@valid_emails) {
	test("MAIL FROM: <$_>", qq/250 Sender <test$i\@mail.ru> Ok/, "Valid MAIL email $_");
	test("RSET", q/250 Ok/, "RSET for $_");
	++$i;
}

test("MAIL FROM:<>", q/250 Sender <> Ok/, "Valid empty sender");
test("MAIL FROM:<test\@mail.ru>", q/421 Command out of sequence/, "MAIL second try");
test("RSET", q/250 Ok/, "RSET for second try");
test("MAIL FROM:<test\@mail.ru>", q/250 Sender <test\@mail.ru> Ok/, "MAIL second try after RSET");

for (@invalid_emails) {
	test("RCPT TO:<$_>", q/500 Syntax error/, "Invalid RCPT email: $_");
}

$i = 0;
for (@valid_emails) {
	test("RCPT TO:<$_>", qq/250 Recipient <test$i\@mail.ru> Ok/, "Valid RCPT email $_");
	++$i;
}

$sock->close;

print_stat();

1;
