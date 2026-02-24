\version "2.24.2"
\header {
  title = "Purcell bars 48-52"
}

%% Solidus glyph for passing notes in figured bass.
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
    %% 1. Melody (treble)
    \new Staff = "melody" {
      \clef treble
      \key g \major
      \time 3/2
      g'2. b'4 b'4 cis''4
    }
    %% 2. High realization notes (treble)
    \new Staff = "real-treble" {
      \clef treble
      \key g \major
      \time 3/2
      <b' g' d'>2~ <b' g' d'>2 <g' cis'>2
    }
    %% 3. BASS_ACTUAL (voice 1) + low realization notes (voice 2)
    \new Staff = "real-bass" {
      \clef bass
      \key g \major
      \time 3/2
      <<
        \new Voice = "bass-actual" {
          \voiceOne
          g2 fis2 e2
        }
        \new Voice = "real-bass-notes" {
          \voiceTwo
          s2 g2 s2
        }
      >>
    }
    %% 4. Written/expected bass note + figured bass below
    \new Staff = "bass" {
      \clef bass
      \key g \major
      \time 3/2
      <<
        \new Voice = "bass-expected" {
          g2 fis2 e2
        }
        \new FiguredBass {
          \figuremode {
            <_>2 <_\\>2 <6+>2
          }
        }
      >>
    }
  >>
}