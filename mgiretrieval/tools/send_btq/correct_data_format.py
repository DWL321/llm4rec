
with open('caption_data/photo_dense_caption.txt', 'r', encoding='utf-8') as f:
    for line in f:
        if 'photo_id:' in line:
            line = line.strip().replace(' ', '')
            parts = line.split(',')
            pid = parts[0].split(':')[1]
            cap = parts[1].split(':')[1]
            with open('caption_data/photo_dense_caption_corr.txt', 'a') as fo:
                fo.write("{}\t{}\n".format(pid, cap))
        else:
            with open('caption_data/photo_dense_caption_corr.txt', 'a') as fo:
                fo.write(line)