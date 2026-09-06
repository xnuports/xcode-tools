/*
 * genstrings -- pull the localizable strings out of source and write .strings.
 *
 * Also installed as extractLocStrings, which Apple ship as a symlink to this
 * same program.
 *
 * The scan is textual, not a parse: the tool looks for the name of a
 * localization macro, reads the argument list that follows it and takes the
 * literals out of it.  That is not an oversight to be improved on -- it is
 * the behaviour, and Apple's does the same.  A call that has been commented
 * out is extracted like any other, because nothing here knows what a comment
 * is, and a project relying on that to suppress a string would find Apple's
 * genstrings extracting it too.
 *
 * Two things Apple's does that this does not, both of which show up only in
 * source written to provoke them.  Their diagnostics run ahead of their
 * guard, so a function named check_NSLocalizedString draws a complaint
 * about its argument list even though nothing is extracted from it; and
 * where one key collects several comments, the order those are listed in is
 * not the order the files gave them, which suggests they are held in a set.
 * The strings written out are what this matches.
 *
 * A macro name has to stand alone to be a call: MyNSLocalizedString and
 * NSLocalizedStringX are not NSLocalizedString, and neither Apple's tool nor
 * this one extracts from them.  Apple's diagnostics are looser than their
 * extraction and complain about those anyway; the strings they write are
 * what this matches, not the complaints.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#import <Foundation/Foundation.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char	*progname;
static bool		 opt_quiet;		/* -q */
static bool		 opt_positional = true;	/* -noPositionalParameters */
static bool		 opt_append;		/* -a */
static bool		 opt_bigendian;		/* -bigEndian */

/* The comment written for a call that passed nil where a comment goes. */
static NSString * const kNoComment =
    @"No comment provided by engineer.";

static void
usage(void)
{
	printf("Usage: genstrings [OPTION] file1.{m,c,cpp,swift} ... "
	    "filen.{m,c,cpp,swift}\n");
	printf("\n");
	printf("Options\n");
	printf(" -h                       shows this message and exits.\n");
	printf(" -encoding encoding       assume input files use specified "
	    "encoding.\n");
	printf(" -SwiftUI                 enables SwiftUI Text() support.\n");
	printf(" -a                       append output to the old strings "
	    "files.\n");
	printf(" -s substring             substitute 'substring' for "
	    "NSLocalizedString.\n");
	printf(" -skipTable tablename     skip over the file for "
	    "'tablename'.\n");
	printf(" -table tablename         only process certain tables. \n");
	printf(" -noPositionalParameters  turns off positional parameter "
	    "support.\n");
	printf(" -u                       allow unicode characters.\n");
	printf(" -macRoman                read files as MacRoman not UTF-8.\n");
	printf(" -d                       attempt to detect encoding if read "
	    "fails.\n");
	printf(" -q                       turns off multiple key/value pairs "
	    "warning.\n");
	printf(" -bigEndian               output generated with big endian "
	    "byte order.\n");
	printf(" -littleEndian            output generated with little endian "
	    "byte order.\n");
	printf(" -o dir                   place output files in 'dir'.\n");
	printf("\n");
	printf("Please see the genstrings(1) man page for full "
	    "documentation\n");
	printf("\n");
}

@interface Entry : NSObject
@property (nonatomic, copy) NSString *key;
@property (nonatomic, copy) NSString *value;	/* nil: the key is the value */
@property (nonatomic, strong) NSMutableArray<NSString *> *comments;
@end

@implementation Entry
@end

/*
 * One localization macro: the name to look for and where its arguments are.
 * Argument positions are one-based; zero means the macro does not take that
 * one.  Swift spells the same calls with labels, which are honoured when
 * present and these positions used when they are not.
 */
struct macro {
	const char	*suffix;	/* appended to the base name */
	int		 key, table, value, comment;
	bool		 core_foundation;
};

static const struct macro macros[] = {
	{ "WithDefaultValue",		1, 2, 4, 5, false },
	{ "FromTableInBundle",		1, 2, 0, 4, false },
	{ "FromTable",			1, 2, 0, 3, false },
	{ "",				1, 0, 0, 2, false },
	{ "WithDefaultValue",		1, 2, 4, 5, true },
	{ "FromTableInBundle",		1, 2, 0, 4, true },
	{ "FromTable",			1, 2, 0, 3, true },
	{ "",				1, 0, 0, 2, true },
};
static const size_t nmacros = sizeof(macros) / sizeof(macros[0]);

static bool
ident_char(unichar c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') || c == '_');
}

/*
 * Read a string literal at *pos, in any of the spellings the three languages
 * use: "...", @"..." and CFSTR("..."), with adjacent literals joined.
 *
 * The text between the quotes is taken exactly as written -- a \n in the
 * source stays two characters, and a newline that was really in the source
 * stays a newline.  That is what Apple's writes into the .strings file, and
 * it is the right answer anyway: the escape belongs to the string, and
 * decoding and re-encoding it could only lose.
 *
 * Returns nil when what is there is not a literal, which is the one thing
 * this tool refuses to guess about.
 */
static NSString *
literal_at(NSString *s, NSUInteger *pos)
{
	NSMutableString *out = nil;
	NSUInteger i = *pos, n = s.length;
	bool any = false;

	for (;;) {
		bool cfstr = false;
		NSUInteger start;

		while (i < n && [[NSCharacterSet whitespaceAndNewlineCharacterSet]
		    characterIsMember:[s characterAtIndex:i]])
			i++;
		if (i < n && [s characterAtIndex:i] == '@')
			i++;
		else if (i + 6 < n && [[s substringWithRange:
		    NSMakeRange(i, 6)] isEqualToString:@"CFSTR("]) {
			i += 6;
			cfstr = true;
			while (i < n && [s characterAtIndex:i] == ' ')
				i++;
		}
		if (i >= n || [s characterAtIndex:i] != '"')
			break;
		i++;
		if (out == nil)
			out = [NSMutableString string];
		start = i;
		while (i < n) {
			unichar c = [s characterAtIndex:i];

			if (c == '\\' && i + 1 < n) {
				i += 2;
				continue;
			}
			if (c == '"')
				break;
			i++;
		}
		[out appendString:[s substringWithRange:
		    NSMakeRange(start, i - start)]];
		if (i < n)
			i++;		/* past the closing quote */
		any = true;
		if (cfstr) {
			while (i < n && [s characterAtIndex:i] == ' ')
				i++;
			if (i < n && [s characterAtIndex:i] == ')')
				i++;
		}
		{	/* Another literal butted against this one joins it. */
			NSUInteger j = i;

			while (j < n && [[NSCharacterSet
			    whitespaceAndNewlineCharacterSet]
			    characterIsMember:[s characterAtIndex:j]])
				j++;
			if (j < n && ([s characterAtIndex:j] == '"' ||
			    [s characterAtIndex:j] == '@'))
				continue;
		}
		break;
	}
	if (!any)
		return (nil);
	*pos = i;
	return (out);
}

/*
 * Split the argument list that starts at the open parenthesis, tracking
 * nesting and string literals so that a comma inside either does not end an
 * argument.  Returns the arguments as written, and leaves *pos past the
 * closing parenthesis.
 */
static NSArray<NSString *> *
arguments_at(NSString *s, NSUInteger *pos)
{
	NSMutableArray *args = [NSMutableArray array];
	NSUInteger i = *pos, n = s.length, start;
	int depth = 0;

	if (i >= n || [s characterAtIndex:i] != '(')
		return (nil);
	i++;
	start = i;
	while (i < n) {
		unichar c = [s characterAtIndex:i];

		if (c == '"') {
			i++;
			while (i < n) {
				unichar d = [s characterAtIndex:i];

				if (d == '\\') {
					i += 2;
					continue;
				}
				i++;
				if (d == '"')
					break;
			}
			continue;
		}
		if (c == '(' || c == '[' || c == '{')
			depth++;
		else if (c == ')' || c == ']' || c == '}') {
			if (depth == 0 && c == ')') {
				[args addObject:[s substringWithRange:
				    NSMakeRange(start, i - start)]];
				*pos = i + 1;
				return (args);
			}
			depth--;
		} else if (c == ',' && depth == 0) {
			[args addObject:[s substringWithRange:
			    NSMakeRange(start, i - start)]];
			start = i + 1;
		}
		i++;
	}
	return (nil);
}

/* Strip a Swift argument label, returning the label or nil. */
static NSString *
split_label(NSString *arg, NSString **rest)
{
	NSUInteger i = 0, n = arg.length;

	while (i < n && [[NSCharacterSet whitespaceAndNewlineCharacterSet]
	    characterIsMember:[arg characterAtIndex:i]])
		i++;
	{
		NSUInteger start = i;

		while (i < n && ident_char([arg characterAtIndex:i]))
			i++;
		if (i > start && i < n && [arg characterAtIndex:i] == ':') {
			*rest = [arg substringFromIndex:i + 1];
			return ([arg substringWithRange:
			    NSMakeRange(start, i - start)]);
		}
	}
	*rest = arg;
	return (nil);
}

static bool
is_nil(NSString *arg)
{
	NSString *t = [arg stringByTrimmingCharactersInSet:
	    [NSCharacterSet whitespaceAndNewlineCharacterSet]];

	return ([t isEqualToString:@"nil"] || [t isEqualToString:@"NULL"] ||
	    [t isEqualToString:@"Nil"] || [t isEqualToString:@".none"] ||
	    t.length == 0);
}

/*
 * %@ and %d become %1$@ and %2$d, so that a translation may reorder them.
 * Only worth doing where there is something to reorder: a lone specifier is
 * left as it was, and so is a string that already numbers its own.
 */
static NSString *
positionalize(NSString *s)
{
	NSMutableString *out = [NSMutableString string];
	NSUInteger i = 0, n = s.length;
	int count = 0, index = 0;

	for (i = 0; i < n; i++) {
		unichar c = [s characterAtIndex:i];

		if (c != '%')
			continue;
		if (i + 1 < n && [s characterAtIndex:i + 1] == '%') {
			i++;
			continue;
		}
		if (i + 1 < n && [s characterAtIndex:i + 1] >= '1' &&
		    [s characterAtIndex:i + 1] <= '9')
			return (s);	/* already numbered */
		count++;
	}
	if (count < 2)
		return (s);

	for (i = 0; i < n; i++) {
		unichar c = [s characterAtIndex:i];

		[out appendFormat:@"%C", c];
		if (c != '%')
			continue;
		if (i + 1 < n && [s characterAtIndex:i + 1] == '%') {
			[out appendString:@"%"];
			i++;
			continue;
		}
		[out appendFormat:@"%d$", ++index];
	}
	return (out);
}

int
main(int argc, char *argv[])
{
@autoreleasepool {
	NSMutableArray<NSString *> *files = [NSMutableArray array];
	NSMutableDictionary<NSString *,
	    NSMutableDictionary<NSString *, Entry *> *> *tables =
	    [NSMutableDictionary dictionary];
	NSMutableArray<Entry *> *ordered = [NSMutableArray array];
	NSMutableArray<NSString *> *only = [NSMutableArray array];
	NSMutableArray<NSString *> *skip = [NSMutableArray array];
	NSString *base = @"NSLocalizedString", *outdir = @".";
	NSStringEncoding encoding = NSUTF8StringEncoding;
	bool detect = false;
	int i;

	/* Apple ship this program twice; it answers to whichever name. */
	progname = strrchr(argv[0], '/');
	progname = progname != NULL ? progname + 1 : argv[0];
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "-h") == 0) {
			usage();
			return (0);
		}
		if (strcmp(a, "-q") == 0)
			opt_quiet = true;
		else if (strcmp(a, "-a") == 0)
			opt_append = true;
		else if (strcmp(a, "-u") == 0 || strcmp(a, "-SwiftUI") == 0)
			;	/* accepted; nothing here restricts either */
		else if (strcmp(a, "-noPositionalParameters") == 0)
			opt_positional = false;
		else if (strcmp(a, "-bigEndian") == 0)
			opt_bigendian = true;
		else if (strcmp(a, "-littleEndian") == 0)
			opt_bigendian = false;
		else if (strcmp(a, "-macRoman") == 0)
			encoding = NSMacOSRomanStringEncoding;
		else if (strcmp(a, "-d") == 0)
			detect = true;
		else if (strcmp(a, "-s") == 0 && i + 1 < argc)
			base = [NSString stringWithUTF8String:argv[++i]];
		else if (strcmp(a, "-o") == 0 && i + 1 < argc)
			outdir = [NSString stringWithUTF8String:argv[++i]];
		else if (strcmp(a, "-table") == 0 && i + 1 < argc)
			[only addObject:[NSString
			    stringWithUTF8String:argv[++i]]];
		else if (strcmp(a, "-skipTable") == 0 && i + 1 < argc)
			[skip addObject:[NSString
			    stringWithUTF8String:argv[++i]]];
		else if (strcmp(a, "-encoding") == 0 && i + 1 < argc)
			i++;	/* named encodings are Apple's own list */
		else if (a[0] == '-')
			;	/* unknown options are ignored, as Apple's are */
		else
			[files addObject:[NSString stringWithUTF8String:a]];
	}
	if (files.count == 0) {
		usage();
		return (0);
	}

	for (NSString *path in files) {
		NSString *text = [NSString stringWithContentsOfFile:path
		    encoding:encoding error:NULL];
		NSUInteger at = 0;

		if (text == nil && detect)
			text = [NSString stringWithContentsOfFile:path
			    usedEncoding:NULL error:NULL];
		if (text == nil) {
			fprintf(stderr, "%s: error: can't read %s\n", progname,
			    [path UTF8String]);
			continue;
		}

		while (at < text.length) {
			NSRange found;
			const struct macro *m = NULL;
			NSString *name = nil;
			NSArray<NSString *> *args;
			NSUInteger pos, k;
			NSString *key = nil, *value = nil, *comment = nil;
			NSString *table = @"Localizable";
			bool bad = false;
			size_t mi;

			/*
			 * Find whichever macro comes next.  Longest name
			 * first, so that FromTableInBundle is not read as
			 * FromTable with a stray bundle after it.
			 */
			found.location = NSNotFound;
			for (mi = 0; mi < nmacros; mi++) {
				NSString *cand = macros[mi].core_foundation ?
				    [@"CFCopyLocalizedString" stringByAppendingString:
				    [NSString stringWithUTF8String:
				    macros[mi].suffix]] :
				    [base stringByAppendingString:
				    [NSString stringWithUTF8String:
				    macros[mi].suffix]];
				NSRange r = [text rangeOfString:cand options:0
				    range:NSMakeRange(at, text.length - at)];

				if (r.location == NSNotFound)
					continue;
				if (found.location != NSNotFound &&
				    r.location > found.location)
					continue;
				if (found.location == r.location &&
				    cand.length <= name.length)
					continue;
				found = r;
				name = cand;
				m = &macros[mi];
			}
			if (found.location == NSNotFound)
				break;

			pos = NSMaxRange(found);
			/* The name must stand alone, not end a longer one. */
			if (found.location > 0 &&
			    ident_char([text characterAtIndex:
			    found.location - 1])) {
				at = pos;
				continue;
			}
			while (pos < text.length &&
			    [[NSCharacterSet whitespaceAndNewlineCharacterSet]
			    characterIsMember:[text characterAtIndex:pos]])
				pos++;
			if (pos >= text.length ||
			    [text characterAtIndex:pos] != '(') {
				at = pos;
				continue;
			}
			if ((args = arguments_at(text, &pos)) == nil) {
				at = NSMaxRange(found);
				continue;
			}
			at = pos;

			for (k = 0; k < args.count; k++) {
				NSString *rest = nil;
				NSString *label = split_label(args[k], &rest);
				int slot = (int)k + 1;
				NSString * __strong *dest = NULL;

				if (label != nil) {
					if ([label isEqualToString:@"tableName"])
						dest = &table;
					else if ([label isEqualToString:@"value"])
						dest = &value;
					else if ([label isEqualToString:@"comment"])
						dest = &comment;
					else if ([label isEqualToString:@"bundle"])
						continue;
				}
				if (dest == NULL) {
					if (slot == m->key)
						dest = &key;
					else if (slot == m->table)
						dest = &table;
					else if (slot == m->value)
						dest = &value;
					else if (slot == m->comment)
						dest = &comment;
					else
						continue;
				}
				if (is_nil(rest)) {
					if (dest == &table)
						table = @"Localizable";
					continue;
				}
				{
					NSUInteger p = 0;
					NSString *lit = literal_at(rest, &p);

					/*
					 * An empty key is no key: Apple
					 * report it the same way they report
					 * an argument that is not a literal
					 * at all, and so does this.
					 */
					if (lit != nil && dest == &key &&
					    lit.length == 0)
						lit = nil;
					if (lit == nil) {
						NSUInteger line = 1, c;

						for (c = 0;
						    c < found.location; c++) {
							if ([text characterAtIndex:c]
							    == '\n')
								line++;
						}
						fprintf(stderr, "%s: error: bad "
						    "entry in file %s (line = "
						    "%lu): %s is not a literal "
						    "string.\n",
						    progname, [path UTF8String],
						    (unsigned long)line,
						    dest == &table ? "Table" :
						    "Argument");
						bad = true;
						break;
					}
					*dest = lit;
				}
			}
			/*
			 * Every one of these macros ends in a comment, so a
			 * call that stops short of it is malformed however
			 * good the arguments it did pass look.
			 */
			if (!bad && m->comment > 0 &&
			    (int)args.count < m->comment) {
				NSUInteger line = 1, c;

				for (c = 0; c < found.location; c++) {
					if ([text characterAtIndex:c] == '\n')
						line++;
				}
				fprintf(stderr, "%s: error: bad entry in file "
				    "%s (line = %lu): Argument is not a "
				    "literal string.\n", progname,
				    [path UTF8String], (unsigned long)line);
				bad = true;
			}
			if (bad || key == nil)
				continue;
			/*
			 * An empty comment is no comment, and gets the same
			 * placeholder a missing one does.  A comment of
			 * nothing but whitespace is a comment, and Apple
			 * warn about it clashing like any other.
			 */
			if (comment != nil && comment.length == 0)
				comment = nil;
			if (value != nil && value.length == 0)
				value = nil;

			{
				NSMutableDictionary *t = tables[table];
				Entry *e;

				if (t == nil) {
					t = [NSMutableDictionary dictionary];
					tables[table] = t;
				}
				if ((e = t[key]) == nil) {
					e = [Entry new];
					e.key = key;
					e.comments = [NSMutableArray array];
					t[key] = e;
					[ordered addObject:e];
				}
				if (value != nil) {
					if (e.value == nil)
						e.value = value;
					else if (![e.value isEqualToString:value])
						fprintf(stderr, "Key \"%s\" used "
						    "with multiple values. Value "
						    "\"%s\" kept. Value \"%s\" "
						    "ignored.\n",
						    [key UTF8String],
						    [e.value UTF8String],
						    [value UTF8String]);
				}
				if (comment != nil &&
				    ![e.comments containsObject:comment])
					[e.comments addObject:comment];
			}
		}
	}

	/*
	 * The clashing-comment warnings come after every file has been read,
	 * in the order the keys were first seen, and name all the comments a
	 * key collected rather than just the first pair.
	 */
	if (!opt_quiet) {
		for (Entry *e in ordered) {
			NSMutableString *list;

			if (e.comments.count < 2)
				continue;
			list = [NSMutableString string];
			for (NSString *c in e.comments)
				[list appendFormat:@"%@\"%@\"",
				    list.length ? @" & " : @"", c];
			fprintf(stderr, "%s: warning: Key \"%s\" used with "
			    "multiple comments %s\n", progname,
			    [e.key UTF8String], [list UTF8String]);
		}
	}

	for (NSString *table in tables) {
		NSMutableDictionary<NSString *, Entry *> *t = tables[table];
		NSMutableString *out = [NSMutableString string];
		NSString *path;
		NSArray *keys;
		NSData *data;

		if (only.count > 0 && ![only containsObject:table])
			continue;
		if ([skip containsObject:table])
			continue;

		keys = [[t allKeys] sortedArrayUsingComparator:
		    ^NSComparisonResult(NSString *a, NSString *b) {
			return ([a caseInsensitiveCompare:b]);
		}];
		for (NSString *key in keys) {
			Entry *e = t[key];
			NSString *value = e.value != nil ? e.value : e.key;

			if (opt_positional)
				value = positionalize(value);
			[out appendFormat:@"/* %@ */\n",
			    e.comments.count == 0 ? kNoComment :
			    [e.comments componentsJoinedByString:@"\n   "]];
			[out appendFormat:@"\"%@\" = \"%@\";\n\n",
			    e.key, value];
		}

		path = [outdir stringByAppendingPathComponent:
		    [table stringByAppendingPathExtension:@"strings"]];
		data = [out dataUsingEncoding:opt_bigendian ?
		    NSUTF16BigEndianStringEncoding :
		    NSUTF16LittleEndianStringEncoding];
		{
			/* .strings files carry a byte order mark. */
			NSMutableData *withBOM = [NSMutableData data];
			const uint8_t be[2] = { 0xfe, 0xff };
			const uint8_t le[2] = { 0xff, 0xfe };

			[withBOM appendBytes:opt_bigendian ? be : le length:2];
			[withBOM appendData:data];
			data = withBOM;
		}
		if (opt_append) {
			NSFileHandle *fh = [NSFileHandle
			    fileHandleForWritingAtPath:path];

			if (fh != nil) {
				[fh seekToEndOfFile];
				[fh writeData:data];
				[fh closeFile];
				continue;
			}
		}
		if (![data writeToFile:path atomically:NO])
			fprintf(stderr, "%s: error: can't write %s\n", progname,
			    [path UTF8String]);
	}
	return (0);
}
}
