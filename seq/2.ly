\version "2.22.1"
\header {
  title = "An Evening Hymn"
  composer = "Henry Purcell"
}

%% Solidus glyph for passing notes in figured bass.
%% A simple diagonal stroke, sized and sloped to match the figure column.
passingNoteSolidus = \markup
  \override #'(thickness . 1.4)
  \draw-line #'(0.55 . 1.4)

#(define-public (continuo-format-bass-figure figure event context)
   (let ((slash (eq? #t (ly:event-property event 'augmented-slash))))
     (if (and slash (not (number? figure)))
         passingNoteSolidus
         (format-bass-figure figure event context))))

\layout {
  \context {
    \Score
    figuredBassFormatter = #continuo-format-bass-figure
  }
}

\score {
  <<
    \new Staff = "melody" {
      \clef treble
      \key g \major
      \time 3/2
      \set Score.currentBarNumber = #38
      \override Score.BarNumber.break-visibility = ##(#f #f #t)
      \override Score.BarNumber.self-alignment-X = #CENTER
      \set Score.barNumberVisibility = #all-bar-numbers-visible
      fis'1 r2 a'2. g'4 fis'2 d'1 b'2 b'2 e'2 a'2 g'4 fis'4 e'2. d'4
    }
    \new Staff = "continuo" {
      \clef bass
      \key g \major
      \time 3/2
      <<
        \new Voice = "bassline" {
          d2 cis2 b,2 cis2 b,2 a,2 b,2 a,2 g,2 a,2 g,2 fis,2 g,2 a,2 a,2
        }
        \new FiguredBass {
          \figuremode {
            <_>2 <_\\>2 <6>2 <6>2 <_\\>2 <3+ 6>2 <_>2 <_\\>2 <6>2 <_+>2 <_\\>2 <6>2 <6>2 <4>2 <_+>2
          }
        }
      >>
    }
  >>
}